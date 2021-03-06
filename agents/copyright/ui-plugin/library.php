<?php
/***********************************************************
 Copyright (C) 2010, 2011 Hewlett-Packard Development Company, L.P.

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
***********************************************************/

/************************************************************
  This file contains common functions for the
  copyright ui plugin.
 ************************************************************/

/***********************************************************
 Sort query histogram results (by content)
 ***********************************************************/
function hist_rowcmp($rowa, $rowb)
{
  return (strnatcasecmp($rowa['content'], $rowb['content']));
}

/***********************************************************
 Sort rows by filename
 ***********************************************************/
function copyright_namecmp($rowa, $rowb)
{
  return (strnatcasecmp($rowa['ufile_name'], $rowb['ufile_name']));
}


/*
 * Return files with a given copyright.
 * Inputs:
 *   $agent_pk
 *   $hash            content hash
 *   $type            content type (statement, url, email)
 *   $uploadtree_pk   sets scope of request
 *   $PkgsOnly        if true, only list packages, default is false (all files are listed)
 *                    $PkgsOnly is not yet implemented.
 *   $offset          select offset, default is 0
 *   $limit           select limit (num rows returned), default is no limit
 *   $order           sql order by clause, default is blank
 *                      e.g. "order by ufile_name asc"
 * Returns:
 *   pg_query result.  See $sql for fields returned.
 *   Caller should use pg_free_result to free.
 */
function GetFilesWithCopyright($agent_pk, $hash, $type, $uploadtree_pk, 
                             $PkgsOnly=false, $offset=0, $limit="ALL",
                             $order="")
{
  global $PG_CONN;
  
  /* Find lft and rgt bounds for this $uploadtree_pk  */
  $sql = "SELECT lft, rgt, upload_fk FROM uploadtree 
                 WHERE uploadtree_pk = $uploadtree_pk";
  $result = pg_query($PG_CONN, $sql);
  DBCheckResult($result, $sql, __FILE__, __LINE__);
  $row = pg_fetch_assoc($result);
  $lft = $row["lft"];
  $rgt = $row["rgt"];
  $upload_pk = $row["upload_fk"];
  pg_free_result($result);

  $sql = "select distinct uploadtree_pk, pfile_fk, ufile_name
          from copyright,
              (SELECT pfile_fk as PF, uploadtree_pk, ufile_name from uploadtree 
                 where upload_fk=$upload_pk
                   and uploadtree.lft BETWEEN $lft and $rgt) as SS
          where PF=pfile_fk and agent_fk=$agent_pk
                and hash='$hash' and type='$type'
          group by uploadtree_pk, pfile_fk, ufile_name 
          $order limit $limit offset $offset";
  $result = pg_query($PG_CONN, $sql);  // Top uploadtree_pk's
  DBCheckResult($result, $sql, __FILE__, __LINE__);

//echo "<br>$sql<br>";
  return $result;
}

/*
 * Count files (pfiles) with a given copyright string.
 * Inputs:
 *   $agent_pk
 *   $hash            content hash
 *   $type            content type (statement, url, email)
 *   $uploadtree_pk   sets scope of request
 *   $PkgsOnly        if true, only list packages, default is false (all files are listed)
 *                    $PkgsOnly is not yet implemented.  Default is false.
 *   $CheckOnly       if true, sets LIMIT 1 to check if uploadtree_pk has 
 *                    any of the given copyrights.  Default is false.
 * Returns:
 *   Number of unique pfiles with $hash
 */
function CountFilesWithCopyright($agent_pk, $hash, $type, $uploadtree_pk, 
                             $PkgsOnly=false, $CheckOnly=false)
{
  global $PG_CONN;
  
  /* Find lft and rgt bounds for this $uploadtree_pk  */
  $sql = "SELECT lft, rgt, upload_fk FROM uploadtree 
                 WHERE uploadtree_pk = $uploadtree_pk";
  $result = pg_query($PG_CONN, $sql);
  DBCheckResult($result, $sql, __FILE__, __LINE__);
  $row = pg_fetch_assoc($result);
  $lft = $row["lft"];
  $rgt = $row["rgt"];
  $upload_pk = $row["upload_fk"];
  pg_free_result($result);

  $chkonly = ($CheckOnly) ? " LIMIT 1" : "";

  $sql = "SELECT count(DISTINCT pfile_fk) as unique from copyright,
            (SELECT pfile_fk as PF from uploadtree 
                where upload_fk=$upload_pk and uploadtree.lft BETWEEN $lft and $rgt) as SS
            where PF=pfile_fk and agent_fk=$agent_pk and hash='$hash' and type='$type' 
            $chkonly";

  $result = pg_query($PG_CONN, $sql);  // Top uploadtree_pk's
  DBCheckResult($result, $sql, __FILE__, __LINE__);
  
  $row = pg_fetch_assoc($result);
  $FileCount = $row['unique'];
  pg_free_result($result);
  return $FileCount;
}


  /***********************************************************
   StmtReorder(): 
   rearrange copyright statment to try and put the holder first,
   followed by the rest of the statement.
   For example,
     copyright (c) aaron seigo <aseigo kde.org>
   would reorder to
     aaron seigo <aseigo kde.org> | copyright (c)
   this way the output will be better grouped by author.
   ***********************************************************/
  function StmtReorder($content)
  {
    // NOT YET IMPLEMENTED
    return $content;
  }


  /***********************************************************
   MassageContent(): 
   Input row array contains: pfile, content and type
   Output records: massaged content, type, hash
                   where content has been simplified from
                   the raw records and hash is the md5 of this
                   new content.
   If $hash non zero, only rows with that hash will
   be returned.
   On empty row, return true, else false
   ***********************************************************/
  function MassageContent(&$row, $hash)
  {
    /* Step 1: Clean up content
     */
    $OriginalContent = $row['content'];

    /* remove control characters */
    $content = preg_replace('/[\x0-\x1f]/', ' ', $OriginalContent);   

    if ($row['type'] == 'statement')
    {
      /* !"#$%&' */
      $content = preg_replace('/([\x21-\x27])|([*@])/', ' ', $content);   

      /*  numbers-numbers, two or more digits, ', ' */
      $content = preg_replace('/(([0-9]+)-([0-9]+))|([0-9]{2,})|(,)/', ' ', $content);  
      $content = preg_replace('/ : /', ' ', $content);  // free :, probably followed a date
    }
    else
    if ($row['type'] == 'email')
    {
      $content = str_replace(":;<=>()", " ", $content);
    }

    /* remove double spaces */
    $content = preg_replace('/\s\s+/', ' ', $content);

    /* remove leading/trailing whitespace and some punctuation */
    $content = trim($content, "\t \n\r<>./\"\'");

    /* remove leading "dnl " */
    if ((strlen($content) > 4) &&
        (substr_compare($content, "dnl ", 0, 4, true) == 0))
      $content = substr($content, 4);

    /* skip empty content */
    if (empty($content)) return true;

    /* Step 1B: rearrange copyright statments to try and put the holder first,
     * followed by the rest of the statement, less copyright years.
     */
/* Not yet implemented
      if ($row['type'] == 'statement') $content = $this->StmtReorder($content);
*/

    //  $row['original'] = $OriginalContent;   // to compare original to new content
    $row['content'] = $content; 
    $row['copyright_count'] = 1;
    $row['hash'] = md5($row['content']);
    if ($hash && ($row['hash'] != $hash)) return true;

    return false;
  }  /* End of MassageContent() */

?>
