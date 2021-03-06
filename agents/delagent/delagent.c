/********************************************************
 delagent: Remove an upload from the DB and repository

 Copyright (C) 2007 Hewlett-Packard Development Company, L.P.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License along
 with this program; if not, write to the Free Software Foundation, Inc.,
 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 ********************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <signal.h>
#include <libgen.h>

#include <libfossdb.h>
#include <libfossrepo.h>
#include "libfossagent.h"


#ifdef SVN_REV
char BuildVersion[]="Build version: " SVN_REV ".\n";
#endif

int Verbose=0;
int Test=0;
#define MAXSQL	1024
#define MAXLINE	1024

/* for DB */
void	*DB=NULL;
int	Agent_pk=-1;	/* agent ID */
char	SQL[MAXSQL];

/* For heartbeats */
long	ItemsProcessed=0;


/*********************************************
 MyDBaccess(): DBaccess with debugging.
 *********************************************/
int	MyDBaccess	(void *V, char *S)
{
  int rc;
  if (Verbose > 1) printf("%s\n",S);
  rc = DBaccess(V,S);
  if (rc < 0)
	{
	fprintf(stderr,"FATAL: SQL failed: '%s'.\n",SQL);
	DBclose(DB);
	exit(-1);
	}
  return(rc);
} /* MyDBaccess() */

/*********************************************
 DeleteLicense(): Given an upload ID, delete all
 licenses associated with it.
 The DoBegin flag determines whether BEGIN/COMMIT
 should be called.
 Do this if you want to reschedule license analysis.
 *********************************************/
void	DeleteLicense	(long UploadId)
{
  void *VDB;
  char TempTable[256];

  if (Verbose) { printf("Deleting licenses for upload %ld\n",UploadId); }
  DBaccess(DB,"SET statement_timeout = 0;"); /* no timeout */
  MyDBaccess(DB,"BEGIN;");
  memset(TempTable,'\0',sizeof(TempTable));
  snprintf(TempTable,sizeof(TempTable),"DelLic_%ld",UploadId);

  /* Create the temp table */
  if (Verbose) { printf("# Creating temp table: %s\n",TempTable); }
  memset(SQL,'\0',sizeof(SQL));

  /* Get the list of pfiles to process */
  memset(SQL,'\0',sizeof(SQL));
  snprintf(SQL,sizeof(SQL),"SELECT DISTINCT(pfile_fk) FROM uploadtree WHERE upload_fk = '%ld' ;",UploadId);
  MyDBaccess(DB,SQL);
  VDB = DBmove(DB);

  /***********************************************/
  /* delete pfile licenses */
  if (Verbose) { printf("# Deleting licenses\n"); }
      memset(SQL,'\0',sizeof(SQL));
      snprintf(SQL,sizeof(SQL),"DELETE FROM licterm_name WHERE pfile_fk IN (SELECT pfile_fk FROM uploadtree WHERE upload_fk = '%ld');",UploadId);
      MyDBaccess(DB,SQL);

      memset(SQL,'\0',sizeof(SQL));
      snprintf(SQL,sizeof(SQL),"DELETE FROM agent_lic_status WHERE pfile_fk IN (SELECT pfile_fk FROM uploadtree WHERE upload_fk = '%ld');",UploadId);
      MyDBaccess(DB,SQL);

      memset(SQL,'\0',sizeof(SQL));
      snprintf(SQL,sizeof(SQL),"DELETE FROM agent_lic_meta WHERE pfile_fk IN (SELECT pfile_fk FROM uploadtree WHERE upload_fk = '%ld');",UploadId);
      MyDBaccess(DB,SQL);
  ItemsProcessed+=DBdatasize(VDB);
  Heartbeat(ItemsProcessed);

  /***********************************************/
  /* Commit the change! */
  if (Verbose) { printf("# Delete completed\n"); }
  if (Test) MyDBaccess(DB,"ROLLBACK;");
  else
	{
	MyDBaccess(DB,"COMMIT;");
#if 0
	/** Disabled: DB will take care of this **/
	if (Verbose) { printf("# Running vacuum and analyze\n"); }
	MyDBaccess(DB,"VACUUM ANALYZE agent_lic_status;");
	MyDBaccess(DB,"VACUUM ANALYZE agent_lic_meta;");
#endif
	}
  DBaccess(DB,"SET statement_timeout = 120000;");

  DBclose(VDB);
  if (ItemsProcessed > 0)
	{
	/* use heartbeat to say how many are completed */
	raise(SIGALRM);
	}
  if (Verbose) { printf("Deleted licenses for upload %ld\n",UploadId); }
} /* DeleteLicense() */

/*********************************************
 DeleteUpload(): Given an upload ID, delete it.
 *********************************************/
void	DeleteUpload	(long UploadId)
{
  void *VDB;
  char *S;
  int Row,MaxRow;
  char TempTable[256];

  if (Verbose) { printf("Deleting upload %ld\n",UploadId); }
  DBaccess(DB,"SET statement_timeout = 0;"); /* no timeout */
  MyDBaccess(DB,"BEGIN;");
  memset(TempTable,'\0',sizeof(TempTable));
  snprintf(TempTable,sizeof(TempTable),"DelUp_%ld",UploadId);

  /***********************************************/
  /*** Delete everything that impacts the UI ***/
  /***********************************************/

  /***********************************************/
  /* Delete the upload from the folder-contents table */
  memset(SQL,'\0',sizeof(SQL));
  if (Verbose) { printf("# Deleting foldercontents\n"); }
  snprintf(SQL,sizeof(SQL),"DELETE FROM foldercontents WHERE (foldercontents_mode & 2) != 0 AND child_id = %ld;",UploadId);
  MyDBaccess(DB,SQL);

  if (!Test)
	{
	/* The UI depends on uploadtree and folders for navigation.
	   Delete them now to block timeouts from the UI. */
	if (Verbose) { printf("# COMMIT;\n"); }
	MyDBaccess(DB,"COMMIT;");
	if (Verbose) { printf("# BEGIN;\n"); }
	MyDBaccess(DB,"BEGIN;");
	}


  /***********************************************/
  /*** Begin complicated stuff ***/
  /***********************************************/

  /* Get the list of pfiles to delete */
  /** These are all pfiles in the upload_fk that only appear once. **/
  memset(SQL,'\0',sizeof(SQL));
  if (Verbose) { printf("# Getting list of pfiles to delete\n"); }
  snprintf(SQL,sizeof(SQL),"SELECT DISTINCT pfile_pk,pfile_sha1 || '.' || pfile_md5 || '.' || pfile_size AS pfile INTO %s_pfile FROM uploadtree INNER JOIN pfile ON upload_fk = %ld AND pfile_fk = pfile_pk;",TempTable,UploadId);
  MyDBaccess(DB,SQL);

  /* Remove pfiles with reuse */
  memset(SQL,'\0',sizeof(SQL));
  snprintf(SQL,sizeof(SQL),"DELETE FROM %s_pfile USING uploadtree WHERE pfile_pk = uploadtree.pfile_fk AND uploadtree.upload_fk != %ld;",TempTable,UploadId);
  MyDBaccess(DB,SQL);

  memset(SQL,'\0',sizeof(SQL));
  snprintf(SQL,sizeof(SQL),"SELECT COUNT(*) FROM %s_pfile;",TempTable);
  MyDBaccess(DB,SQL);
  if (Verbose) { printf("# Created pfile table: %ld entries\n",atol(DBgetvalue(DB,0,0))); }

  /* Get the file listing -- needed for deleting pfiles from the repository. */
  memset(SQL,'\0',sizeof(SQL));
  snprintf(SQL,sizeof(SQL),"SELECT * FROM %s_pfile ORDER BY pfile_pk;",TempTable);
  MyDBaccess(DB,SQL);
  VDB = DBmove(DB);
  MaxRow = DBdatasize(VDB);

  /***********************************************
   This begins the slow part that locks the DB.
   The problem is, we don't want to lock a critical row,
   otherwise the scheduler will lock and/or fail.
   ***********************************************/

  /***********************************************/
  /* delete pfile references from all the pfile dependent tables */
  if (Verbose) { printf("# Deleting from licterm_name\n"); }
  memset(SQL,'\0',sizeof(SQL));
  snprintf(SQL,sizeof(SQL),"DELETE FROM licterm_name USING %s_pfile WHERE pfile_fk = pfile_pk;",TempTable);
  MyDBaccess(DB,SQL);

  if (Verbose) { printf("# Deleting from agent_lic_status\n"); }
  memset(SQL,'\0',sizeof(SQL));
  snprintf(SQL,sizeof(SQL),"DELETE FROM agent_lic_status USING %s_pfile WHERE pfile_fk = pfile_pk;",TempTable);
  MyDBaccess(DB,SQL);

  if (Verbose) { printf("# Deleting from agent_lic_meta\n"); }
  memset(SQL,'\0',sizeof(SQL));
  snprintf(SQL,sizeof(SQL),"DELETE FROM agent_lic_meta USING %s_pfile WHERE pfile_fk = pfile_pk;",TempTable);
  MyDBaccess(DB,SQL);

  if (Verbose) { printf("# Deleting from attrib\n"); }
  memset(SQL,'\0',sizeof(SQL));
  snprintf(SQL,sizeof(SQL),"DELETE FROM attrib USING %s_pfile WHERE pfile_fk = pfile_pk;",TempTable);
  MyDBaccess(DB,SQL);

  /* delete package info table */
  if (Verbose) { printf("# Deleting pkg_deb_req\n"); }
  memset(SQL,'\0',sizeof(SQL));
  snprintf(SQL,sizeof(SQL),"DELETE FROM pkg_deb_req USING pkg_deb,%s_pfile WHERE pkg_fk = pkg_pk AND pfile_fk = pfile_pk;",TempTable);
  MyDBaccess(DB,SQL);

  if (Verbose) { printf("# Deleting pkg_deb\n"); }
  memset(SQL,'\0',sizeof(SQL));
  snprintf(SQL,sizeof(SQL),"DELETE FROM pkg_deb USING %s_pfile WHERE pfile_fk = pfile_pk;",TempTable);
  MyDBaccess(DB,SQL);

  if (Verbose) { printf("# Deleting pkg_rpm_req\n"); }
  memset(SQL,'\0',sizeof(SQL));
  snprintf(SQL,sizeof(SQL),"DELETE FROM pkg_rpm_req USING pkg_rpm,%s_pfile WHERE pkg_fk = pkg_pk AND pfile_fk = pfile_pk;",TempTable);
  MyDBaccess(DB,SQL);

  if (Verbose) { printf("# Deleting pkg_rpm\n"); }
  memset(SQL,'\0',sizeof(SQL));
  snprintf(SQL,sizeof(SQL),"DELETE FROM pkg_rpm USING %s_pfile WHERE pfile_fk = pfile_pk;",TempTable);
  MyDBaccess(DB,SQL);

  /* delete pfiles in the license_file table */
  if (Verbose) { printf("# Deleting from licese_file\n"); }
  memset(SQL,'\0',sizeof(SQL));
  snprintf(SQL,sizeof(SQL),"DELETE FROM license_file USING %s_pfile "
	   "WHERE pfile_fk = pfile_pk;",TempTable);
  MyDBaccess(DB,SQL);

  /* Delete pfile reference from tag and tag_file table */
  memset(SQL,'\0',sizeof(SQL));
  snprintf(SQL,sizeof(SQL),"SELECT DISTINCT tag_fk, pfile_fk INTO temp_tag_file FROM tag_file INNER JOIN %s_pfile ON pfile_fk = pfile_pk;",TempTable);
  MyDBaccess(DB,SQL);

  if (Verbose) { printf("# Deleting tag_file\n"); }
  memset(SQL,'\0',sizeof(SQL));
  snprintf(SQL,sizeof(SQL),"DELETE FROM tag_file USING %s_pfile WHERE pfile_fk = pfile_pk;",TempTable);
  MyDBaccess(DB,SQL);
  if (Verbose) { printf("# Deleting tag\n"); }
  memset(SQL,'\0',sizeof(SQL));
  snprintf(SQL,sizeof(SQL),"DELETE FROM tag USING temp_tag_file,%s_pfile WHERE tag_fk = tag_pk AND pfile_fk = pfile_pk;",TempTable);
  MyDBaccess(DB,SQL);
  memset(SQL,'\0',sizeof(SQL));
  snprintf(SQL,sizeof(SQL),"DROP TABLE temp_tag_file;");
  MyDBaccess(DB,SQL);

  /***********************************************/
  /*** Everything above is slow, everything below is fast ***/
  /***********************************************/

  /* Delete upload from nomos_ars */
  memset(SQL,'\0',sizeof(SQL));
  if (Verbose) { printf("# Deleting nomos_ars\n"); }
  snprintf(SQL,sizeof(SQL),"DELETE FROM nomos_ars WHERE upload_fk = %ld;",UploadId);
  MyDBaccess(DB,SQL);

  /* Delete upload reference from bucket_ars */
  memset(SQL,'\0',sizeof(SQL));
  if (Verbose) { printf("# Deleting bucket_ars\n"); }
  snprintf(SQL,sizeof(SQL),"DELETE FROM bucket_ars WHERE upload_fk = %ld;",UploadId);
  MyDBaccess(DB,SQL);
  /* Delete uploadtree reference from bucket_container  */
  memset(SQL,'\0',sizeof(SQL));
  if (Verbose) { printf("# Deleting bucket_container\n"); }
  snprintf(SQL,sizeof(SQL),"DELETE FROM bucket_container USING uploadtree WHERE uploadtree_fk = uploadtree_pk AND upload_fk = %ld;",UploadId);
  MyDBaccess(DB,SQL);
  /* Delete pfile reference from bucket_file */
  if (Verbose) { printf("# Deleting bucket_file\n"); }
  memset(SQL,'\0',sizeof(SQL));
  snprintf(SQL,sizeof(SQL),"DELETE FROM bucket_file USING %s_pfile WHERE pfile_fk = pfile_pk;",TempTable);
  MyDBaccess(DB,SQL);

  /* Delete pfile reference from copyright table */
  if (Verbose) { printf("# Deleting copyright\n"); }
  memset(SQL,'\0',sizeof(SQL));
  snprintf(SQL,sizeof(SQL),"DELETE FROM copyright USING %s_pfile WHERE pfile_fk = pfile_pk;",TempTable);
  MyDBaccess(DB,SQL);

  /***********************************************/
  /* Blow away jobs */
  /*****
   There is an ordering issue.
   The delete from attrib and pfile can take a long time (hours for
   a source code DVD).
   If we delete from the jobqueue first, then it will hang the scheduler
   as the scheduler tries to update the jobqueue record.  (Row is locked
   by the delete process.)
   The solution is to delete the jobqueue LAST, so the scheduler won't hang.
   *****/
  memset(SQL,'\0',sizeof(SQL));
  if (Verbose) { printf("# Deleting jobdepends\n"); }
  snprintf(SQL,sizeof(SQL),"DELETE FROM jobdepends USING jobqueue,job WHERE jdep_jq_fk = jq_pk AND jq_job_fk = job_pk AND job_upload_fk = %ld;",UploadId);
  MyDBaccess(DB,SQL);

  memset(SQL,'\0',sizeof(SQL));
  if (Verbose) { printf("# Deleting jobqueue\n"); }
  snprintf(SQL,sizeof(SQL),"DELETE FROM jobqueue USING job WHERE jq_job_fk = job_pk AND job_upload_fk = %ld;",UploadId);
  MyDBaccess(DB,SQL);

  memset(SQL,'\0',sizeof(SQL));
  if (Verbose) { printf("# Deleting job\n"); }
  snprintf(SQL,sizeof(SQL),"DELETE FROM job WHERE job_upload_fk = %ld;",UploadId);
  MyDBaccess(DB,SQL);

  /***********************************************/
  /* Delete the actual upload */
  memset(SQL,'\0',sizeof(SQL));
  if (Verbose) { printf("# Deleting uploadtree\n"); }
  snprintf(SQL,sizeof(SQL),"DELETE FROM uploadtree WHERE upload_fk = %ld;",UploadId);
  MyDBaccess(DB,SQL);

  memset(SQL,'\0',sizeof(SQL));
  if (Verbose) { printf("# Deleting upload\n"); }
  snprintf(SQL,sizeof(SQL),"DELETE FROM upload WHERE upload_pk = %ld;",UploadId);
  MyDBaccess(DB,SQL);

  /***********************************************/
  /* Commit the change! */
  if (Test)
	{
	if (Verbose) { printf("# ROLLBACK\n"); }
	MyDBaccess(DB,"ROLLBACK;");
	}
  else
	{
	if (Verbose) { printf("# COMMIT\n"); }
	MyDBaccess(DB,"COMMIT;");
#if 0
	/** Disabled: Database will take care of this **/
	if (Verbose) { printf("# VACUUM and ANALYZE\n"); }
	MyDBaccess(DB,"VACUUM ANALYZE agent_lic_status;");
	MyDBaccess(DB,"VACUUM ANALYZE agent_lic_meta;");
	MyDBaccess(DB,"VACUUM ANALYZE attrib;");
	MyDBaccess(DB,"VACUUM ANALYZE pfile;");
	MyDBaccess(DB,"VACUUM ANALYZE foldercontents;");
	MyDBaccess(DB,"VACUUM ANALYZE upload;");
	MyDBaccess(DB,"VACUUM ANALYZE uploadtree;");
	MyDBaccess(DB,"VACUUM ANALYZE jobdepends;");
	MyDBaccess(DB,"VACUUM ANALYZE jobqueue;");
	MyDBaccess(DB,"VACUUM ANALYZE job;");
#endif
	}


  /***********************************************/
  /* delete from pfile is SLOW due to constraint checking.
     Do it separately. */
  if (Verbose) { printf("# Deleting from pfile\n"); }
  memset(SQL,'\0',sizeof(SQL));
  snprintf(SQL,sizeof(SQL),"DELETE FROM pfile USING %s_pfile WHERE pfile.pfile_pk = %s_pfile.pfile_pk;",TempTable,TempTable);
  MyDBaccess(DB,SQL);
  memset(SQL,'\0',sizeof(SQL));
  snprintf(SQL,sizeof(SQL),"DROP TABLE %s_pfile;",TempTable);
  MyDBaccess(DB,SQL);

  DBaccess(DB,"SET statement_timeout = 120000;");

  if (Verbose) { printf("Deleted upload %ld from DB, now doing repository.\n",UploadId); }

  /***********************************************/
  /* Whew!  Now to delete the actual pfiles from the repository. */
  /** If someone presses ^C now, then at least the DB is accurate. **/
  if (Test <= 1)
    {
    for(Row=0; Row<MaxRow; Row++)
      {
      memset(SQL,'\0',sizeof(SQL));
      S = DBgetvalue(VDB,Row,1); /* sha1.md5.len */
      if (RepExist("license",S))
	{
	if (Test) printf("TEST: Delete %s %s\n","license",S);
	else RepRemove("license",S);
	}
      if (RepExist("files",S))
	{
	if (Test) printf("TEST: Delete %s %s\n","files",S);
	else RepRemove("files",S);
	}
      if (RepExist("gold",S))
	{
	if (Test) printf("TEST: Delete %s %s\n","gold",S);
	else RepRemove("gold",S);
	}
      ItemsProcessed++;
      Heartbeat(ItemsProcessed);
      }
    } /* if Test <= 1 */
  DBclose(VDB);
  if (ItemsProcessed > 0)
	{
	/* use heartbeat to say how many are completed */
	raise(SIGALRM);
	}
  if (Verbose) { printf("Deleted upload %ld\n",UploadId); }
} /* DeleteUpload() */

/*********************************************
 ListFoldersRecurse(): Draw folder tree.
 if DelFlag is set, then all child uploads are
 deleted and the folders are deleted.
 *********************************************/
void	ListFoldersRecurse	(void *VDB, long Parent, int Depth,
				 int Row, int DelFlag)
{
  int r,MaxRow;
  long Fid;
  int i;
  char *Desc;

  /* Find all folders with this parent and recurse */
  MaxRow = DBdatasize(VDB);
  for(r=0; r < MaxRow; r++)
    {
    if (r == Row) continue; /* skip self-loops */
    /* NOTE: There can be an infinite loop if two rows point to each other.
       A->parent == B and B->parent == A  */
    if (atol(DBgetvalue(VDB,r,1)) == Parent)
	{
	if (!DelFlag)
		{
		for(i=0; i<Depth; i++) fputs("   ",stdout);
		}
	Fid = atol(DBgetvalue(VDB,r,0));
	if (Fid != 0)
		{
		if (!DelFlag)
			{
			printf("%4ld :: %s",Fid,DBgetvalue(VDB,r,2));
			Desc = DBgetvalue(VDB,r,3);
			if (Desc && Desc[0]) printf(" (%s)",Desc);
			printf("\n");
			}
		ListFoldersRecurse(VDB,Fid,Depth+1,r,DelFlag);
		}
	else
		{
		if (DelFlag) DeleteUpload(atol(DBgetvalue(VDB,r,4)));
		else printf("%4s :: Contains: %s\n","--",DBgetvalue(VDB,r,2));
		}
	}
    }

  /* if we're deleting folders, do it now */
  if (DelFlag)
	{
	switch(Parent)
	  {
	  case 1:	/* skip default parent */
		printf("INFO: Default folder not deleted.\n");
		break;
	  case 0:	/* it's an upload */
		break;
	  default:	/* it's a folder */
		memset(SQL,'\0',sizeof(SQL));
		snprintf(SQL,sizeof(SQL),"DELETE FROM foldercontents WHERE foldercontents_mode = 1 AND child_id = '%ld';",Parent);
		if (Test) printf("TEST: %s\n",SQL);
		else MyDBaccess(DB,SQL);

		memset(SQL,'\0',sizeof(SQL));
		snprintf(SQL,sizeof(SQL),"DELETE FROM folder WHERE folder_pk = '%ld';",Parent);
		if (Test) printf("TEST: %s\n",SQL);
		else MyDBaccess(DB,SQL);
		break;
	  } /* switch() */
	}
} /* ListFoldersRecurse() */

/*********************************************
 ListFolders(): List every folder.
 *********************************************/
void	ListFolders	()
{
  int i,j,MaxRow;
  long Fid;	/* folder ids */
  int DetachFlag=0;
  int Match;
  char *Desc;
  void *VDB;

  printf("# Folders\n");
  MyDBaccess(DB,"SELECT folder_pk,parent,name,description,upload_pk FROM folderlist ORDER BY name,parent,folder_pk;");
  VDB = DBmove(DB);
  ListFoldersRecurse(VDB,1,1,-1,0);

  /* Find detached folders */
  MaxRow = DBdatasize(VDB);
  DetachFlag=0;
  for(i=0; i < MaxRow; i++)
      {
      Fid = atol(DBgetvalue(VDB,i,1));
      if (Fid == 1) continue;	/* skip default parent */
      Match=0;
      for(j=0; (j<MaxRow) && !Match; j++)
	{
	if ((i!=j) && (atol(DBgetvalue(VDB,j,0)) == Fid)) Match=1;
	}
      if (!Match && !atol(DBgetvalue(VDB,i,4)))
	{
	if (!DetachFlag) { printf("# Unlinked folders\n"); DetachFlag=1; }
	printf("%4ld :: %s",Fid,DBgetvalue(VDB,i,2));
	Desc = DBgetvalue(VDB,i,3);
	if (Desc && Desc[0]) printf(" (%s)",Desc);
	printf("\n");
	ListFoldersRecurse(VDB,Fid,1,i,0);
	}
      }

  /* Find detached uploads */
  DetachFlag=0;
  for(i=0; i < MaxRow; i++)
      {
      Fid = atol(DBgetvalue(VDB,i,1));
      if (Fid == 1) continue;	/* skip default parent */
      Match=0;
      for(j=0; (j<MaxRow) && !Match; j++)
	{
	if ((i!=j) && (atol(DBgetvalue(VDB,j,0)) == Fid)) Match=1;
	}
      if (!Match && atol(DBgetvalue(VDB,i,4)))
	{
	if (!DetachFlag) { printf("# Unlinked uploads (uploads without folders)\n"); DetachFlag=1; }
	printf("%4s",DBgetvalue(VDB,i,4));
	printf(" :: %s",DBgetvalue(VDB,i,2));
	Desc = DBgetvalue(VDB,i,3);
	if (Desc && Desc[0]) printf(" (%s)",Desc);
	printf("\n");
	}
      }

  DBclose(VDB);
} /* ListFolders() */

/*********************************************
 ListUploads(): List every upload ID.
 *********************************************/
void	ListUploads	()
{
  int Row,MaxRow;
  long NewPid;

  printf("# Uploads\n");
  MyDBaccess(DB,"SELECT upload_pk,upload_desc,upload_filename FROM upload ORDER BY upload_pk;");

  /* list each value */
  MaxRow = DBdatasize(DB);
  for(Row=0; Row < MaxRow; Row++)
      {
      NewPid = atol(DBgetvalue(DB,Row,0));
      if (NewPid >= 0)
	{
	char *S;
	printf("%ld :: %s",NewPid,DBgetvalue(DB,Row,2));
	S = DBgetvalue(DB,Row,1);
	if (S && S[0]) printf(" (%s)",S);
	printf("\n");
	}
      }
} /* ListUploads() */

/*********************************************
 DeleteFolder(): Given a folder ID, delete it
 AND recursively delete everything below it!
 This includes upload deletion!
 *********************************************/
void	DeleteFolder	(long FolderId)
{
  void *VDB;
  MyDBaccess(DB,"SELECT folder_pk,parent,name,description,upload_pk FROM folderlist ORDER BY name,parent,folder_pk;");
  VDB = DBmove(DB);
  ListFoldersRecurse(VDB,FolderId,0,-1,1);
  DBclose(VDB);
#if 0
  /** Disabled: Database will take care of this **/
  MyDBaccess(DB,"VACUUM ANALYZE foldercontents;");
  MyDBaccess(DB,"VACUUM ANALYZE folder;");
#endif
} /* DeleteFolder() */

/**********************************************************************/
/**********************************************************************/
/**********************************************************************/

/**********************************************
 ReadFileLine(): Read a single line from a file.
 Used to read from stdin.
 Process line elements.
 Returns: 1 of read data, 0=no data, -1=EOF.
 NOTE: It only returns 1 if a filename changes!
 **********************************************/
int     ReadFileLine        (FILE *Fin)
{
  int C='@';
  int i=0;      /* index */
  char FullLine[MAXLINE];
  char *L;
  int rc=0;     /* assume no data */
  int Type=0;	/* 0=undefined; 1=delete; 2=list */
  int Target=0;	/* 0=undefined; 1=upload; 2=license; 3=folder */
  long Id;

  memset(FullLine,0,MAXLINE);
  /* inform scheduler that we're ready for data */
  printf("OK\n");
  alarm(60);
  fflush(stdout);

  if (feof(Fin))
    {
    return(-1);
    }

  /* read a line */
  while(!feof(Fin) && (i < MAXLINE-1) && (C != '\n') && (C>0))
    {
    C=fgetc(Fin);
    if ((C>0) && (C!='\n'))
      {
      FullLine[i]=C;
      i++;
      }
    else if ((C=='\n') && (i==0))
      {
      C='@';  /* ignore blank lines */
      }
    }
  if ((i==0) && feof(Fin)) return(-1);
  if (Verbose > 1) fprintf(stderr,"DEBUG: Line='%s'\n",FullLine);

  /* process the line. */
  L = FullLine;
  while(isspace(L[0])) L++;

  /** Get the type of command: delete or list **/
  if (!strncasecmp(L,"DELETE",6) && isspace(L[6]))
	{
	Type=1; /* delete */
	L+=6;
	}
  else if (!strncasecmp(L,"LIST",4) && isspace(L[4]))
	{
	Type=2; /* list */
	L+=4;
	}
  while(isspace(L[0])) L++;
  /** Get the target **/
  if (!strncasecmp(L,"UPLOAD",6) && (isspace(L[6]) || !L[6]))
	{
	Target=1; /* upload */
	L+=6;
	}
  else if (!strncasecmp(L,"LICENSE",7) && (isspace(L[7]) || !L[7]))
	{
	Target=2; /* license */
	L+=7;
	}
  else if (!strncasecmp(L,"FOLDER",6) && (isspace(L[6]) || !L[6]))
	{
	Target=3; /* folder */
	L+=6;
	}
  while(isspace(L[0])) L++;
  Id = atol(L);

  /* Handle the request */
  if ((Type==1) && (Target==1))	{ DeleteUpload(Id); rc=1; }
  else if ((Type==1) && (Target==2))	{ DeleteLicense(Id); rc=1; }
  else if ((Type==1) && (Target==3))	{ DeleteFolder(Id); rc=1; }
  else if ((Type==2) && (Target==1))	{ ListUploads(); rc=1; }
  else if ((Type==2) && (Target==2))	{ ListUploads(); rc=1; }
  else if ((Type==2) && (Target==3))	{ ListFolders(); rc=1; }
  else
    {
    printf("ERROR: Unknown command: '%s'\n",FullLine);
    }

  return(rc);
} /* ReadFileLine() */


/**********************************************************************/
/**********************************************************************/
/**********************************************************************/

/*********************************************
 Usage():
 *********************************************/
void	Usage	(char *Name)
{
  fprintf(stderr,"Usage: %s [options]\n",Name);
  fprintf(stderr,"  List or delete uploads.\n");
  fprintf(stderr,"  Options\n");
  fprintf(stderr,"  -i   :: Initialize the DB, then exit.\n");
  fprintf(stderr,"  -u   :: List uploads IDs.\n");
  fprintf(stderr,"  -U # :: Delete upload ID.\n");
  fprintf(stderr,"  -l   :: List uploads IDs. (same as -u, but goes with -L)\n");
  fprintf(stderr,"  -L # :: Delete ALL licenses associated with upload ID.\n");
  fprintf(stderr,"  -f   :: List folder IDs.\n");
  fprintf(stderr,"  -F # :: Delete folder ID and all uploads under this folder.\n");
  fprintf(stderr,"          Folder '1' is the default folder.  '-F 1' will delete\n");
  fprintf(stderr,"          every upload and folder in the navigation tree.\n");
  fprintf(stderr,"  -s   :: Run from the scheduler.\n");
  fprintf(stderr,"  -T   :: TEST -- do not update the DB or delete any files (just pretend)\n");
  fprintf(stderr,"  -v   :: Verbose (-vv for more verbose)\n");
} /* Usage() */

/**********************************************************************/
int	main	(int argc, char *argv[])
{
  int c;
  int ListProj=0, ListFolder=0;
  long DelUpload=0, DelFolder=0, DelLicense=0;
  int Scheduler=0; /* should it run from the scheduler? */
  int GotArg=0;
  char *agent_desc = "Deletes upload.  Other list/delete options available from the command line.";

  while((c = getopt(argc,argv,"ifF:lL:sTuU:v")) != -1)
    {
    switch(c)
      {
      case 'i':
	DB = DBopen();
	if (!DB)
	  {
	  fprintf(stderr,"ERROR: Unable to open DB\n");
	  exit(-1);
	  }
	GetAgentKey(DB, basename(argv[0]), 0, SVN_REV, agent_desc);
	DBclose(DB);
	return(0);
      case 'f': ListFolder=1; GotArg=1; break;
      case 'F': DelFolder=atol(optarg); GotArg=1; break;
      case 'L': DelLicense=atol(optarg); GotArg=1; break;
      case 'l': ListProj=1; GotArg=1; break;
      case 's': Scheduler=1; GotArg=1; break;
      case 'T': Test++; break;
      case 'u': ListProj=1; GotArg=1; break;
      case 'U': DelUpload=atol(optarg); GotArg=1; break;
      case 'v': Verbose++; break;
      default:	Usage(argv[0]); exit(-1);
      }
    }

  if (!GotArg)
    {
    Usage(argv[0]);
    exit(-1);
    }

  DB = DBopen();
  if (!DB)
	{
	fprintf(stderr,"ERROR: Unable to open DB\n");
	exit(-1);
	}
  GetAgentKey(DB, basename(argv[0]), 0, SVN_REV, agent_desc);
  signal(SIGALRM,ShowHeartbeat);

  if (ListProj) ListUploads();
  if (ListFolder) ListFolders();

  alarm(60);  /* from this point on, handle the alarm */
  if (DelUpload) { DeleteUpload(DelUpload); }
  if (DelFolder) { DeleteFolder(DelFolder); }
  if (DelLicense) { DeleteLicense(DelLicense); }

  /* process from the scheduler */
  if (Scheduler)
    {
    while(ReadFileLine(stdin) >= 0) ;
    }

  DBclose(DB);
  return(0);
} /* main() */

