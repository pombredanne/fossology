/***************************************************************
 Copyright (C) 2006-2009 Hewlett-Packard Development Company, L.P.

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

***************************************************************/
/* Equivalent to core nomos v1.48 */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <signal.h>
#include <libgen.h>

#include "nomos.h"
#include "licenses.h"
#include "util.h"
#include "list.h"
#include "nomos_regex.h"
#include "parse.h"
#include "_autodefs.h"

#define	HASHES		"#####################"
#define	DEBCPYRIGHT	"debian/copyright"

static void prettyPrint(FILE *, char *, int);
static void makeLicenseSummary(list_t *, int, char *, int);
static void noLicenseFound();
#ifdef notdef
static void licenseStringChecks();
static void findLines(char *, char *, int, int, list_t *);
#endif /* notdef */
static int searchStrategy(int, char *, int);
static void saveLicenseData(scanres_t *, int, int, int);
static int scoreCompare(const void *, const void *);

static char miscbuf[myBUFSIZ];
static char any[6];
static char some[7];
static char few[6];
static char year[7];

#ifdef	MEMSTATS
extern void memStats();
#endif	/* MEMSTATS */
#ifdef	STOPWATCH
DECL_TIMER;
int timerBytes;
char timerName[64];
#endif	/* STOPWATCH */

#define	MAX(a, b)	((a) > (b) ? a : b)
#define	MIN(a, b)	((a) < (b) ? a : b)

void licenseInit()
{
    int i;
    int len;
    int same;
    int ssAbove;
    int ssBelow;
    item_t *p;
    char *cp;
    char buf[myBUFSIZ];

#ifdef	PROC_TRACE
    traceFunc("== licenseInit()\n");
#endif	/* PROC_TRACE */

    strcpy(any, "=ANY=");
    strcpy(some, "=SOME=");
    strcpy(few, "=FEW=");
    strcpy(year, "=YEAR=");
    listInit(&gl.sHash, 0, "search-cache"); /* CDB - Added */

    /*
     * Examine the search strings in licSpec looking for 3 corner-cases
     * to optimize all the regex-searches we'll be making:
     * (a) the seed string is the same as the text-search string
     * (b) the text-search string has length 1 and contents == "."
     * (c) the seed string is the 'null-string' indicator
     */
    for (i = 0; i < NFOOTPRINTS; i++) {
	same = 0;
	len = licSpec[i].seed.csLen;
	if (licSpec[i].text.csData == NULL_STR) {
	    licText[i].tseed = "(null)";
	}
	if ((licSpec[i].text.csLen == 1) &&
	    (*(licSpec[i].text.csData) == '.')) {
	    same++;
	    /*CDB -- CHanged next line to use ! */
	} else if ((licSpec[i].seed.csLen == licSpec[i].text.csLen) &&
		   !memcmp(licSpec[i].seed.csData, licSpec[i].text.csData, len)) {
	    same++;
	}
	/*
	 * Step 1, copy the tseed "search seed", decrypt it, and munge any wild-
	 * cards in the string.  Note that once we eliminate the compile-time
	 * string encryption, we could re-use the same exact data.  In fact, some
	 * day (in our copious spare time), we could effectively remove licSpec.
	 */
#ifdef	FIX_STRINGS
	fixSearchString(buf, sizeof(buf), i, YES);
#endif	/* FIX_STRINGS */

	licText[i].tseed = licSpec[i].seed.csData;

	/*---------------------------------------*/
	/* CDB - This is the code that I inadvertently removed. */
	/*
	 * Step 2, add the search-seed to the search-cache
	 */
	if ((p = listGetItem(&gl.sHash, licText[i].tseed)) == NULL_ITEM) {
	    Fatal("Cannot enqueue search-cache item \"%s\"",
		  licText[i].tseed);
	}
	p->refCount++;

	/*--------------------------------*/

	/*
	 * Step 3, handle special cases of NULL seeds and (regex == seed)
	 */
	if (strcmp(licText[i].tseed, "=NULL=") == 0) {	/* null */
#ifdef	OLD_DECRYPT
	    memFree(licText[i].tseed, MTAG_SEEDTEXT);
#endif	/* OLD_DECRYPT */
	    licText[i].tseed = NULL_STR;
	    licText[i].nAbove = licText->nBelow = -1;
	}
	if (same) {		/* seed == phrase */
	    licText[i].regex = licText[i].tseed;
#if	0
	    ssBelow = searchStrategy(i, buf, NO);
	    licText[i].nBelow = MIN(ssBelow, 2);
#endif
	    licText[i].nAbove = licText[i].nBelow = 0;
	}
	/*
	 * Step 4, decrypt and fix the regex (since seed != regex here).  Once
	 * we have all that, searchStrategy() helps determine how many lines
	 * above and below [the seed] to save -- see findPhrase() for details.
	 */
	else {				/* seed != phrase */
	    len = licSpec[i].text.csLen;
	    memcpy(buf, licSpec[i].text.csData, (size_t) (len+1));
#ifdef	OLD_DECRYPT
	    decrypt(buf, len);
#endif	/* OLD_DECRYPT */
	    ssAbove = searchStrategy(i, buf, YES);
	    ssBelow = searchStrategy(i, buf, NO);
#if	0
	    licText[i].nAbove = MIN(ssAbove, 3);
	    licText[i].nBelow = MIN(ssBelow, 6);
#endif
	    licText[i].nAbove = licText[i].nBelow = 1;	/* for now... */
#ifdef	FIX_STRINGS
	    fixSearchString(buf, sizeof(buf), i, NO);
#endif	/* FIX_STRINGS */
	    licText[i].regex = copyString(buf, MTAG_SRCHTEXT);
	}
	if (p->ssComp < (ssAbove*100)+ssBelow) {
	    p->ssComp = (ssAbove*100)+ssBelow;
	}
	licText[i].compiled = 0;
	licText[i].plain = 1;	/* assume plain-text for now */
    }
    /*
     * Now that we've computed the above- and below-values for license
     * searches, set each of the appropriate entries with the MAX values
     * determined.  Limit 'above' values to 3 and 'below' values to 6.
     *****
     * QUESTION: the above has worked in the past - is it STILL valid?
     */
    for (i = 0; i < NFOOTPRINTS; i++) {
	if (licText[i].tseed == NULL_STR) {
#ifdef	LICENSE_DEBUG
	    Note("License[%d] configured with NULL seed", i);
#endif	/* LICENSE_DEBUG */
	    continue;
	}
	if (licText[i].tseed == licText[i].regex) {
#ifdef	LICENSE_DEBUG
	    Note("License[%d] seed == regex", i);
#endif	/* LICENSE_DEBUG */
	    continue;
	}
	licText[i].nAbove = p->ssComp / 100;
	licText[i].nBelow = p->ssComp % 100;
    }


    /*
     * Finally (if enabled), compare each of the search strings to see if
     * there are duplicates, and determine if some of the regexes can be
     * searched via strstr() (instead of it's slower-but-more-functional
     * regex brethern).
     */
    for (i = 0; i < NFOOTPRINTS; i++) {
	for (cp = _REGEX(i); licText[i].plain && *cp; cp++) {
	    switch (*cp) {
	    case '.': case '*': case '+': case '|':
	    case '[': case ']': case '(': case ')':
	    case '^': case '$': case '?': case ',':
	    case '<': case '>': case '{': case '}':
	    case '\\':
		licText[i].plain = 0;
		break;
	    }
	}
	if (i >= _CR_first && i <= _CR_last) {
	    continue;
	}
    }
    return;
}

/*
 * This function should be called BEFORE the wild-card specifier =ANY=
 * is converted to a REAL regex ".*" (e.g., before fixSearchString())!
 *****
 * ASSUME a "standard line-length" of 50 characters/bytes.  That's
 * likely too small, but err on the side of being too conservative.
 *****
 * determining for the number of text-lines ABOVE involves finding out
 * how far into the 'license footprint' the seed-word resides.  ASSUME
 * a standard line-length of 50 (probably too small, but we'll err on
 * the side of being too conservative.  If the seed isn't IN the regex,
 * assume a generally-bad worst-case and search 2-3 lines above.
 *****
 * determining for the number of text-lines BELOW involves finding out
 * how long the 'license footprint' actually is, plus adding some fudge
 * based on the number of wild-cards in the footprint.
 */

#define	LINE_BYTES	50	/* fudge for punctuation, etc. */
#define	LINE_WORDS	8	/* assume this many words per line */
#define	WC_BYTES	30	/* wild-card counts this many bytes */
#define	WC_WORDS	3	/* wild-card counts this many words */
#define	PUNT_LINES	3	/* if "dunno", guess this line-count */
#define	MIN_LINES	1	/* normal minimum-extra-lines */

static int searchStrategy(int index, char *regex, int aboveCalc)
{
    char *start;
    char *cp;
    char *s;
    char seed[myBUFSIZ];
    int words;
    int lines;
    int bytes;
    int minLines;
    int matchWild;
    int matchSeed;

#ifdef	PROC_TRACE
	traceFunc("== searchStrategy(%d(%s), \"%s\", %d)\n", index,
		  _SEED(index), regex, aboveCalc);
#endif	/* PROC_TRACE */

    s = _SEED(index);
    if (s == NULL_STR || strlen(s) == 0) {
#ifdef	LICENSE_DEBUG
	Note("Lic[%d] has NULL seed", index);
#endif	/* LICENSE_DEBUG */
	return(0);
    }
    if (regex == NULL_STR || strlen(regex) == 0) {
#ifdef	LICENSE_DEBUG
	Assert(NO, "searchStrategy(%d) called with NULL data", index);
#endif	/* LICENSE_DEBUG */
	return(0);
    }
    if (strcmp(s, regex) == 0) {
	return(0);
    }
    bytes = words = lines = 0;
    (void) strcpy(seed, s);
    while (seed[strlen(seed)-1] == ' ') {
	seed[strlen(seed)-1] = NULL_CHAR;
    }
    /* how far ABOVE to look depends on location of the seed in footprint */
    if (aboveCalc) {
	if (strGrep(seed, regex, REG_ICASE) == 0) {
#ifdef	LICENSE_DEBUG
	    printf("DEBUG: seed(%d) no hit in regex!\n", index);
#endif	/* LICENSE_DEBUG */
	    return(PUNT_LINES);	/* guess */
	}
	start = regex;
	cp = start;
	for (minLines = 0; cp != NULL; start = cp + 1) {
	    matchWild = matchSeed = 0;
	    if ((cp = strchr(start, ' ')) != NULL_STR) {
		*cp = NULL_CHAR;
	    }
	    matchWild = (strcmp(start, any) == 0 ||
			 strcmp(start, some) == 0 || strcmp(start, few));
	    matchSeed = strcmp(start, seed) == 0;
	    if (!matchSeed) {
		bytes += (matchWild ? WC_BYTES :
			  strlen(start)+1);
		words += (matchWild ? WC_WORDS : 1);
	    }
	    if (cp != NULL_STR) {
		*cp = ' ';
	    }
	    if (matchSeed) {	/* found seed? */
		break;
	    }
	}
	/* optimization for single-lines: */
	minLines += (words >= LINE_WORDS/2 && words < LINE_WORDS);
	lines = MAX(bytes/LINE_BYTES, words/LINE_WORDS)+minLines;
#ifdef	LICENSE_DEBUG
	printf("ABOVE: .... bytes=%d, words=%d; max(%d,%d)+%d == %d\n",
	       bytes, words, bytes/LINE_BYTES, words/LINE_WORDS,
	       minLines, lines);
#endif	/* LICENSE_DEBUG */
	return(words == 0 ? 0 : lines);
    }
    /* calculate how far below to look -- depends on length of footprint */
    for (minLines = MIN_LINES, cp = start = regex; cp; start = cp+1) {
	matchWild = matchSeed = 0;
	if ((cp = strchr(start, ' ')) != NULL_STR) {
	    *cp = NULL_CHAR;
	}
	matchWild = (strcmp(start, any) == 0 ||
		     strcmp(start, some) == 0 || strcmp(start, few));
	matchSeed = strcmp(start, seed) == 0;
	if (matchSeed) {
	    bytes = words = 0;
	    /*minLines = MIN_LINES+1;*/
	}
	else {
	    bytes += (matchWild ? WC_BYTES : strlen(start)+1);
	    words += (matchWild ? WC_WORDS : 1);
	}
	if (cp != NULL_STR) {
	    *cp = ' ';
	}
    }
    lines = MAX(bytes/LINE_BYTES, words/LINE_WORDS)+minLines;
#ifdef	LICENSE_DEBUG
    printf("BELOW: .... bytes=%d, words=%d; max(%d,%d)+%d == %d\n",
	   bytes, words, bytes/LINE_BYTES, words/LINE_WORDS, minLines, lines);
#endif	/* LICENSE_DEBUG */
    return(lines);
}

#ifdef	FIX_STRINGS
static void fixSearchString(char *s, int size, int i, int wildcardBad)
{
	char *cp;
	int len;
	char wildCard[16];
/* */
#ifdef	PROC_TRACE
	traceFunc("== fixSearchString(\"%s\", %d, %d, %d)\n", s, size, i,
	    wildcardBad);
#endif	/* PROC_TRACE */
/* */
/*
 * Decrypt the text-string and then replace all instances of our wild-
 * card string =ANY= to ".*".  This may appear stupid on the surface,
 * but the string =ANY= is *more* noticable when examining the text
 * than an 'embedded' .* wild-card is.  Same for =SOME=...
 *****
 * Make sure the search string does NOT start with a wild-card; it's not
 * necessary and will probably double execution time.  Once we know the
 * first text is 'not wild', walk through an replace our strange =ANY=
 * wildcards with regex(7) wild-cards. The only magic is to ensure the
 * string doesn't END in a wild-card, either (more performance dumb-ness).
 */
	cp = s;
	while (isspace(*cp)) {
		cp++;
	}
	if (strncmp(cp, any, sizeof(any)-1) == 0 ||
	    strncmp(cp, some, sizeof(some)-1) == 0 ||
	    strncmp(cp, few, sizeof(few)-1) == 0) {
		printf("string %d == \"%s\"\n", i, cp);
		Fatal("Text-spec %d begins with a wild-card", i);
	}
/*
 * We'll replace the string " =ANY=" (6 chars) with ".*" (2 chars).
 * The token MUST OCCUR BY ITSELF (e.g., not a substring)!
 */
	(void) sprintf(wildCard, " %s", any);
	len = strlen(wildCard);
	for (cp = s; strGrep(wildCard, cp, 0); ) {
		if (wildcardBad) {
			Fatal("OOPS, regex %d, wild-card not allowed here", i);
		}
		if (*(cp+cur.regm.rm_eo) == NULL_CHAR) {
			Fatal("String %d ends in a wild-card", i);
		}
		else if (*(cp+cur.regm.rm_eo) == ' ') {
#ifdef	DEBUG
			printf("BEFORE(any): %s\n", s);
#endif	/* DEBUG */
			cp += cur.regm.rm_so;
			*cp++ = '.';
			*cp++ = '*';
			memmove(cp, cp+len-1, strlen(cp+len)+2);
#ifdef	DEBUG
			printf("_AFTER(any): %s\n", s);
#endif	/* DEBUG */
		}
		else {
			Note("Wild-card \"%s\" sub-string, phrase %d", wildCard, i);
			cp += cur.regm.rm_eo;
		}
	}
/*
 * Ditto for replacing " =SOME= " (8 chars) with ".{0,60}" (7 chars)
 */
	(void) sprintf(wildCard, " %s", some);
	len = strlen(wildCard);
	for (cp = s; strGrep(wildCard, cp, 0); ) {
		if (wildcardBad) {
			Fatal("OOPS, regex %d, wild-card not allowed here", i);
		}
		if (*(cp+cur.regm.rm_eo) == NULL_CHAR) {
			Fatal("String %d ends in a wild-card", i);
		}
		else if (*(cp+cur.regm.rm_eo) == ' ') {
#ifdef	DEBUG
			printf("BEFORE(some): %s\n", s);
#endif	/* DEBUG */
			cp += cur.regm.rm_so;
			*cp++ = '.';
			*cp++ = '{';
			*cp++ = '0';
			*cp++ = ',';
			*cp++ = '6';
			*cp++ = '0';
			*cp++ = '}';
			memmove(cp, cp+len-6, strlen(cp+len)+7);
#ifdef	DEBUG
			printf("_AFTER(some): %s\n", s);
#endif	/* DEBUG */
		}
		else {
			Note("Wild-card \"%s\" sub-string, phrase %d", wildCard, i);
			cp += cur.regm.rm_eo;
		}
	}
/*
 * And, same for replacing " =FEW= " (7 chars) with ".{0,15}" (7 chars)
 */
	(void) sprintf(wildCard, " %s", few);
	len = strlen(wildCard);
	for (cp = s; strGrep(wildCard, cp, 0); ) {
		if (wildcardBad) {
			Fatal("OOPS, regex %d, wild-card not allowed here", i);
		}
		if (*(cp+cur.regm.rm_eo) == NULL_CHAR) {
			Fatal("String %d ends in a wild-card", i);
		}
		else if (*(cp+cur.regm.rm_eo) == ' ') {
#ifdef	DEBUG
			printf("BEFORE(few): %s\n", s);
#endif	/* DEBUG */
			cp += cur.regm.rm_so;
			*cp++ = '.';
			*cp++ = '{';
			*cp++ = '0';
			*cp++ = ',';
			*cp++ = '3';
			*cp++ = '0';
			*cp++ = '}';
			memmove(cp, cp+len-6, strlen(cp+len)+7);
#ifdef	DEBUG
			printf("_AFTER(few): %s\n", s);
#endif	/* DEBUG */
		}
		else {
			Note("Wild-card \"%s\" sub-string, phrase %d", wildCard, i);
			cp += cur.regm.rm_eo;
		}
	}
/*
 * AND, replace the string "=YEAR=" with "[12][0-9][0-9][0-9][,- ]*".
 * The former is 6 chars in length, the latter is 24.  We must be careful
 * not to overflow the buffer we're passed.
 */
	len = strlen(year);
	while (strGrep(year, s, 0)) {
		if (strlen(s)+25 >= size) {	/* 24 plus 1(NULL) */
			Fatal("buffer overflow, text-spec %d", i);
		}
		cp = (char *)(s+cur.regm.rm_so);
#ifdef	DEBUG
		printf("BEFORE: %s\n", s);
#endif	/* DEBUG */
		memmove(cp+25, cp+6, strlen(cp+len)+1);	/* was 26, 6 */
		memset(cp+6, '_', 19);
#ifdef	DEBUG
		printf("_MOVED: %s\n", s);
#endif	/* DEBUG */
		*cp = *(cp+4) = *(cp+9) = *(cp+14) = *(cp+19) = '[';
		*(cp+1) = '1';
		*(cp+2) = '2';
		*(cp+5) = *(cp+10) = *(cp+15) = '0';
		*(cp+6) = *(cp+11) = *(cp+16) = '-';
		*(cp+7) = *(cp+12) = *(cp+17) = '9';
		*(cp+3) = *(cp+8) = *(cp+13) = *(cp+18) = *(cp+23) = ']';
		*(cp+20) = ' ';
		*(cp+21) = ',';
		*(cp+22) = '-';
		*(cp+24) = '*';
#ifdef	DEBUG
		printf("_AFTER: %s\n", s);
#endif	/* DEBUG */
	}
	return;
}
#endif	/* FIX_STRINGS */


/*
  We get a list passed in, but it'll always be just a single file
*/
void licenseScan(list_t *l)
{
    int i;
    int c;
    int size;
    int lowWater;
    int lowest;
    int nCand;
    int nSkip;
    char *textp;
    char *cp;
    int counts[NKEYWORDS+1];
    scanres_t *scores;
    scanres_t *scp;
    item_t *p;
#ifdef	SCORE_DEBUG
    FILE *scoreFp;
#endif	/* SCORE_DEBUG */

#ifdef	PROC_TRACE
    traceFunc("== licenseScan(%p, %d)\n", l);
#endif	/* PROC_TRACE */

#ifdef	MEMSTATS
    printf("... allocating %d bytes for scanres_t[] array\n",
	   sizeof(*scp)*l->used);
#endif	/* MEMSTATS */

    scores = (scanres_t *) memAlloc(sizeof(*scp) * l->used, MTAG_SCANRES);
    memset((void *)counts, 0, (size_t)((NKEYWORDS+1)*sizeof(int)));

    /*
      CDB -- Some other part of FOSSology has already decided we
      want to scan this file, so we need to look into removing this
      file scoring stuff.
    */
    /*
     * For EACH file, determine if we want to scan it, and if so, scan
     * the candidate files for keywords (to obtain a "score" -- the higher
     * the score, the more likely it has a real open source license in it).
     *****
     * There are lots of things that will 'disintest' us in a file (below).
     *****
     * PERFORMANCE NOTE: this loop is called 400,000 to 500,000 times
     * when parsing a distribution.  Little slow-downs ADD UP quickly!
     */
#ifdef	STOPWATCH
    timerBytes = 0;
    START_TIMER;
#endif	/* STOPWATCH */
    lowest = nSkip = 0;

    for (scp = scores; (p = listIterate(l)) != NULL_ITEM; scp++) {
#ifdef	QA_CHECKS
	if (p->iFlag) {
	    Assert(NO, "Non-zero iFlag for %s", p->str);
	}
#endif	/* QA_CHECKS */
	/*
	 * Use *relative* pathnames wherever possible -- we'll spend less time in
	 * the kernel looking up inodes and pathname components that way.
	 */
	if (*(p->str) == '/') {
	    (void) strcpy(scp->fullpath, p->str);
	    scp->nameOffset = (size_t) (cur.targetLen+1);
	    cp = scp->fullpath;	/* full pathname */
	} else {
	    (void) sprintf(scp->fullpath, "%s/%s", cur.cwd, p->str);
	    scp->nameOffset = (size_t) (cur.cwdLen+1);
	    cp = p->str;	/* relative path == faster open() */
	}
#ifdef	DEBUG
	printf("licenseScan: scan %s\n",
	       (char *)(scp->fullpath+scp->nameOffset));
#endif	/* DEBUG */
	/*
	 * Zero-length files are of no interest; there's nothing in them!
	 *  CDB - We need to report this error somehow... and clean up
	 *  /tmp/nomos.tmpdir (or equivalent).
	 */
	if ((textp = mmapFile(cp)) == NULL_STR) {
	    perror(cp);
	    fprintf(stderr, "Zero length file: %s\n", cp);
#ifdef	QA_CHECKS
	    Assert(NO, "zero-length: %s", cp);
	    mySystem("ls -l '%s'", cp);
#endif	/* QA_CHECKS */
	    continue;
	}
	if (scp->dataOffset = iMadeThis(textp)) {
	    textp += scp->dataOffset;
	    cur.stbuf.st_size -= scp->dataOffset;
	}
	scp->size = cur.stbuf.st_size; /* Where did this get set ? CDB */
	(void) strcpy(scp->ftype, magic_buffer(gl.mcookie, textp,
					       (size_t) scp->size));
#ifdef	DEBUG
	printf("Magic: %s\n", scp->ftype);
#endif	/* DEBUG */
	/*
	 * Disinterest #3 (discriminate-by-file-content):
	 * Files not of a known-good type (as reported by file(1)/magic(3)) should
	 * also be skipped (some are quite large!).  _UTIL_MAGIC (see _autodata.c)
	 * contains a regex for MOST of the files we're interested in, but there
	 * ARE some exceptions (logged below).
	 *****
	 * exception (A): patch/diff files are sometimes identified as "data".
	 *****
	 * FIX-ME: we don't currently use _UTIL_FILTER, which is set up to
	 * exclude some files by filename.
	 */
	if (idxGrep(_UTIL_MAGIC, scp->ftype, REG_ICASE|REG_EXTENDED)) {
	    for (scp->kwbm = c = 0; c < NKEYWORDS; c++) {
		if (idxGrep(c+_KW_first, textp,
			    REG_EXTENDED|REG_ICASE)) {
		    scp->kwbm |= (1 << c);
		    scp->score++;
#if	(DEBUG > 5)
		    printf("Keyword %d (\"%s\"): YES\n", c, _REGEX(c+_KW_first));
#endif	/* DEBUG > 5 */
		}
	    }
	}
	else {
	    scp->score = 0;
	}
	munmapFile(textp);
#if	(DEBUG > 5)
	printf("%s = %d\n", (char *)(scp->fullpath+scp->nameOffset),
	       scp->score);
#endif	/* DEBUG > 5 */
    }

    c = l->used;

    /*
     * CDB - It is always the case that we are doing one file at a time.
     */

    /*
     * If we were invoked with a single-file-only option, just over-ride the
     * score calculation -- give the file any greater-than-zero score so it
     * appears as a valid candidate.  This is important when the file to be
     * evaluated has no keywords, yet might contain authorship inferences.
     */
    if (scores->score == 0) {
	scores->score = 1;	/* force to be a valid candidate  */
    }

#ifdef	PROC_TRACE
    traceFunc("=> invoking qsort(): callback == scoreCompare()\n");
#endif	/* PROC_TRACE */

    qsort(scores, (size_t) c, sizeof(*scp), scoreCompare);
    /*
     * Set up defaults for the minimum-scores for which we'll save files.
     * Try to ensure a minimum # of license files will be recorded for this
     * source/package (try, don't force it too hard); see if lower scores
     * yield a better fit, but recognize the of a non-license file increases
     * as we lower the bar.
     */
    lowWater = 1;
#ifdef	SCORE_DEBUG
    /*
     * Sanity-check the high-scores versus availability of thesaurus or
     * dictionary files.
     */
    if (scores->score < abs(lowest)) {
	Warn("%s: thesaurus is best (tot %d)!\n", cur.basename, nSkip);
	printf("scores->score %d, lowest %d\n", lowest);
    }
#endif	/* SCORE_DEBUG */
    /*
     * Run through the list once more; this time we record and count the
     * license candidates to process.  License candidates are determined
     * by either (score >= low) *OR* matching a set of filename patterns.
     */
#ifdef	SCORE_DEBUG
    scoreFp = fopenFile("../_scores.ALL", "w");
#endif	/* SCORE_DEBUG */
    for (scp = scores, i = nCand = 0; i < c; i++, scp++) {
	scp->relpath = (char *)(scp->fullpath + scp->nameOffset);
	if (idxGrep(_FN_LICENSEPATT, pathBasename(scp->relpath),
		    REG_ICASE|REG_EXTENDED)) {
	    scp->flag = 1;
	    if (idxGrep(_FN_DEBCPYRT, scp->relpath, REG_ICASE)) {
		scp->flag = 2;
	    }
	}
	else if (scp->score >= lowWater) {
	    scp->flag |= 1;
	}
	/*
	 * So now, save any license candidate EITHER named "debian/copyright*"
	 * OR having a score > 0
	 */
	if (scp->flag == 2 || (scp->score && scp->flag)) {
#if	(DEBUG > 3)
	    printf("%s [score: %d], %07o\n", scp->fullpath,
		   scp->score, scp->kwbm);
#endif	/* DEBUG > 3 */
	    nCand++;
	}
#ifdef	SCORE_DEBUG
	fprintf(scoreFp, "%7d %s\n", scp->score, scp->relpath);
#endif	/* SCORE_DEBUG */
    }
#ifdef	SCORE_DEBUG
    (void) fclose(scoreFp);
#endif	/* SCORE_DEBUG */
#ifdef	STOPWATCH
    END_TIMER;
    (void) sprintf(timerName, "scan+sort+select (%d candidates)", nCand);
    PRINT_TIMER(timerName, 0);
    START_TIMER;	/* stops in saveLicenseData! */
#endif	/* STOPWATCH */
    /*
     * OF SPECIAL INTEREST: saveLicenseData() changes directory (to "..")!!!
     */
    saveLicenseData(scores, nCand, c, lowWater);
    /*
     * At this point, we don't need either the raw-source directory or the
     * unpacked results anymore, so get rid of 'em.
     */
    memFree((char *) scores, "scores table");
    return;
}



/*
 * NOTE: this procedure is a qsort callback that provides a REVERSE
 * integer sort (highest to lowest)
 */
static int scoreCompare(const void *arg1, const void *arg2)
{
    scanres_t *sc1 = (scanres_t *)arg1;
    scanres_t *sc2 = (scanres_t *)arg2;

    if (sc1->score > sc2->score) {
	return(-1);
    } else if (sc1->score < sc2->score) {
	return(1);
    } else if ((sc1->fullpath != NULL_STR) && (sc2->fullpath == NULL_STR)) {
	return(-1);
    } else if ((sc2->fullpath != NULL_STR) && (sc1->fullpath == NULL_STR)) {
	return(1);
    } else {
	return(-strcmp(sc1->fullpath, sc2->fullpath));
    }
}

static void noLicenseFound()
{

#ifdef	PROC_TRACE
    traceFunc("== noLicenseFound\n");
#endif	/* PROC_TRACE */

    (void) strcpy(cur.compLic, LS_NOSUM);
    return;
}


/*
 * This function creates all the license-data in a specific directory.
 *****
 * OF SPECIAL INTEREST: this function changes directory!
 */
/*
  CDB - Some initializations happen here for no particular reason
*/
static void saveLicenseData(scanres_t *scores, int nCand, int nElem,
    int lowWater)
{
    scanres_t *scp;
    int i;
    int c;
    int base;
    int size;
    int highScore = scores->score;
    int fCnt;
    int isML = 0;
    int isPS = 0;
    int offset;
    int idx;
    char *fileName;
    char *textp;
    FILE *linkFp;
    FILE *scoreFp;
    item_t *p;

#ifdef	PROC_TRACE
    traceFunc("== saveLicenseData(%p, %d, %d, %d, %d)\n", scores, nCand,
	      nElem, lowWater);
#endif	/* PROC_TRACE */

    /*
     * Save the necessary licensing information in a list of files...
     */
#ifdef	DEBUG
    printf("saveLicenseData: %d candidates\n", nCand);
#endif	/* DEBUG */

    /*    changeDir("..");*/ /* CDB- Why?!!!! */

    /* BE PERFORMANCE-CONSCIOUS WITHIN THIS LOOP (it runs a LOT!) */
    /*
     * OPTIMIZE-ME: should we store local variables and use lots of
     * registers instead of accessing everything through the scanres
     * array?  We've got to be doing some serious address calculations.
     */
    scoreFp = fopenFile(FILE_SCORES, "w");
    fCnt = 0;
    i = 1;

    /*
     * CDB -- This loop has been modified from the core such that occurrences
     * of "scp->" should be replaced with "scores[idx]."
     */
    for (idx = 0; i <= nCand; idx++) {
	/*
	 * If we didn't flag this file as needing to be saved, ignore it.
	 */
	if (scores[idx].flag == 0) {
	    continue;
	}
	if (scores[idx].flag > 1) {
	    fCnt++;
	}
	fprintf(scoreFp, "%7d %s\n", scores[idx].score, scores[idx].relpath);
	(void) sprintf(scores[idx].linkname, "Link%03d.txt", i++);
	linkFp = fopenFile(scores[idx].linkname, "w+");
	fprintf(linkFp, "[%s]\n%s %s\n", LABEL_ATTR, LABEL_PATH,
		scores[idx].relpath);
#if	DEBUG > 5
	printf("name: %s\n[%s]\n", scores[idx].relpath, scores[idx].fullpath);
#endif	/* DEBUG > 5 */
	/*
	 * Kludge up the pointer to the relative-path in scores[idx].fullpath
	 * so we don't
	 * have to as many directory entries to open each file... this works for
	 * anything EXCEPT 'distribution files'.
	 */
	fileName =  scores[idx].fullpath;
	if (optionIsSet(OPTS_DEBUG)) {
	    printf("File name: %s\n", fileName);
	}
	if ((textp = mmapFile(fileName)) == NULL_STR) {
	    Fatal("Null mmapFile(), path=%s", fileName);
	}
	/* CDB 	size = (int) cur.stbuf.st_size; */
	size = scores[idx].size;
	if (scores[idx].dataOffset) {
	    textp += scores[idx].dataOffset;
	}
	/*
	  CDB - the wordCount function called here currently has the
	  important side-effect of setting nLines and nWords in the
	  global structure "cur".
	*/
	fprintf(linkFp, "Type: %s\n%s %s, %d bytes\n",
		scores[idx].ftype, LABEL_CNTS, wordCount(textp),
		scores[idx].size);
	/*
	 * Report which package (if any) this file came from
	 */
	fprintf(linkFp, "Package: %s\n", STR_NOTPKG);

	/*
	 * construct the list of keywords that matched in licenseScan()
	 */
	fprintf(linkFp, "Score: %d\n", scores[idx].score);
	(void) strcpy(miscbuf, "Matches: ");
	offset = 9;	/* e.g., strlen("Matches: ") */
	for (base = c = 0; c < NKEYWORDS; c++) {
	    if (scores[idx].kwbm & (1 << c)) {
		if (base++) {
		    miscbuf[offset++] = ',';
		    miscbuf[offset++] = ' ';
		}
		offset += sprintf(miscbuf+offset, "%s",
				  _REGEX(c+_KW_first));
	    }
	}
	/*
	 * Since we hard-wire the score of every file (invoked as --file), a score
	 * of 1 could be either 0 or 1, so scores[idx].kwbm tells the real story...
	 */
	if (optionIsSet(OPTS_DEBUG)) {
	    printf("File type: %s\n", scores[idx].ftype);
	    printf("File score: %d (0x%06x)\n",
		   (scores[idx].kwbm ? scores[idx].score : scores[idx].kwbm),
		   scores[idx].kwbm);
	    if (scores[idx].kwbm) {
		printf("%s\n", miscbuf);
	    }
	}
	prettyPrint(linkFp, miscbuf, 3);
	/*
	 * Print the license claim (e.g., what's listed in the package)
	 */
	fprintf(linkFp, "License Claim: %s\n", STR_NOTPKG);
	/*
	 * determine licenses in the file, and record 'em; wrap up by including
	 * the file contents
	 *****
	 * FIX-ME: we should filter some names out like the shellscript does.
	 * For instance, word-spell-dictionary files will score high but will
	 * likely NOT contain a license.  But the shellscript filters these
	 * names AFTER they're already scanned.  Think about it.
	 *****
	 FILTERPATTERNS="(/man|\.[0-9]|\.[0-9][a-z]|rfc[0-9].*|.po|.pot"
	 FILTERPATTERNS="$FILTERPATTERNS|words.*|.*spelling.*|spell)$"
	*/
#if	defined(DEBUG) || defined(DOCTOR_DEBUG) || defined(LTSR_DEBUG)	\
    || defined(BATCH_DEBUG) || defined(PARSE_STOPWATCH) || defined(MEMSTATS) \
    || defined(MEM_DEBUG) || defined(UNKNOWN_CHECK_DEBUG)
	printf("*** PROCESS File: %s\n", scores[idx].relpath);
	printf("... %d bytes, score %d, %s\n", scores[idx].size, scores[idx].score,
	       scores[idx].ftype);
#endif /* DEBUG || DOCTOR_DEBUG || LTSR_DEBUG || BATCH_DEBUG || PARSE_STOPWATCH || MEMSTATS || MEM_DEBUG || defined(UNKNOWN_CHECK_DEBUG)*/
	isML = idxGrep(_UTIL_MARKUP, textp, REG_ICASE|REG_EXTENDED);
#ifdef	DOCTOR_DEBUG
	printf("idxGrep(ML) returns %d\n", isML);
	if (isML) {
	    int n;
	    printf("isMarkUp@%d: [", cur.regm.rm_so);
	    for (n = cur.regm.rm_so; n <= cur.regm.rm_eo; n++) {
		printf("%c", *(textp+n));
	    }
	    printf("]\n");
	}
#endif	/* DOCTOR_DEBUG */
	/*
	 * BUG: When _FTYP_POSTSCR is "(postscript|utf-8 unicode)", the resulting
	 * license-parse yields 'NoLicenseFound' but when both "postscript" and
	 * "utf-8 unicode" are searched independently, parsing definitely finds
	 * quantifiable licenses. WHY?
	 */
	if (idxGrep(_FTYP_POSTSCR, scores[idx].ftype, REG_ICASE) ||
	    idxGrep(_FTYP_UTF8, scores[idx].ftype, REG_ICASE) ||
	    idxGrep(_FTYP_RTF, scores[idx].ftype, REG_ICASE)) {
	    isPS = 1;
	}
#ifdef	DOCTOR_DEBUG
	printf("idxGrep(PS) returns %d\n", isPS);
	if (isPS) {
	    int n;
	    printf("isPostScript@%d: [", cur.regm.rm_so);
	    for (n = cur.regm.rm_so; n <= cur.regm.rm_eo; n++) {
		printf("%c", *(scores[idx].ftype + n));
	    }
	    printf("]\n");
	}
#endif	/* DOCTOR_DEBUG */
	/*
	 * Interesting - copyString(parseLicenses(args), MTAG_FILELIC)...
	 * will randomly segfault on 32-bit Debian releases.  Split the calls.
	 */
	fileName = parseLicenses(textp, size, &scores[idx], isML, isPS);
	scores[idx].licenses = copyString(fileName, MTAG_FILELIC);
#ifdef	QA_CHECKS
	if (fileName == NULL_STR) {
	    Assert(NO, "Expected non-null parseLicenses return!");
	}
	if (scores[idx].licenses == NULL_STR) {
	    Assert(NO, "Expected non-null license summary!");
	}
#endif	/* QA_CHECKS */
#ifdef	STOPWATCH
	timerBytes += size;
#endif	/* STOPWATCH */
#ifdef	FLAG_NO_COPYRIGHT
	if (gl.flags & FL_NOCOPYRIGHT) {
	    p = listGetItem(&cur.nocpyrtList, scores[idx].relpath);
	    p->buf = copyString(scores[idx].linkname, MTAG_PATHBASE);
	    p->num = scores[idx].score;
	}
#endif	/* FLAG_NO_COPYRIGHT */
	fprintf(linkFp, "License(s) Found: %s\n", scores[idx].licenses);
	if (cur.licPara != NULL_STR) {
	    if (gl.flags & FL_FRAGMENT) {
		fprintf(linkFp,
			"[- Pattern(fragment) text -]\n\"%s\"\n",
			cur.licPara);
	    } else {
		fprintf(linkFp,
			"[- Possible license text/snippet -]\n%s\n",
			cur.licPara);
	    }
	    memFree(cur.licPara, MTAG_TEXTPARA);	/* be free! */
	    cur.licPara = NULL_STR;			/* remember */
	}
	fprintf(linkFp, "[%s]\n%s", LABEL_TEXT, textp);
	munmapFile(textp);
	(void) fclose(linkFp);

	/*
	 * Remember this license in this file...
	 */
	p = listGetItem(&cur.lList, scores[idx].licenses);
	p->refCount++;
	/*
	 * Clear out the buffer-offsets list
	 */
#ifdef fix_later
/* CDB - need to move this code to a point after we save the license info */
#ifdef	PHRASE_DEBUG
	listDump(&cur.offList, NO);
#endif	/* PHRASE_DEBUG */
	while ((p = listIterate(&cur.offList)) != 0) {
	    listClear(p->buf, YES);
	}
	listClear(&cur.offList, NO);
#endif /* fix_later */
    }

#ifdef	MEMSTATS
    memStats("saveLicenseData: out of loop");
#endif	/* MEMSTATS */

    (void) fclose(scoreFp);
    listSort(&cur.lList, SORT_BY_COUNT_DSC);

#ifdef	QA_CHECKS
    if (cur.lList.used == 0) {
	Assert(NO, "No entries in license-list");
    }
#endif	/* QA_CHECKS */

    /*
     * Construct a 'computed license'.  Wherever possible, leave off the
     * entries for None and LikelyNot; those are individual-file results
     * and we're making an 'aggregate summary' here.
     */
    if (cur.parseList.used == 0) {
	noLicenseFound();
    } else {
	makeLicenseSummary(&cur.parseList, highScore, cur.compLic,
			   sizeof(cur.compLic));
    }
    if (optionIsSet(OPTS_DEBUG)) {
	printf("==> ");
    }
    /* CDB - Debug code
    listDump(&cur.offList, YES);
    while ((p = listIterate(&cur.offList)) != 0) {
	listDump(p->buf, YES);
    }
    */
    printf("LOG: File %s contains license(s) %s\n",
	   basename(cur.targetFile), cur.compLic);
    return;
} /* saveLicenseData */


/*
 * pretty-print the keywords into possibly _several_ formatted lines
 * e.g., 'fmt -t'
 */
#define	_PP_LINESIZE	72	/* formatting limit */

static void prettyPrint(FILE *fp, char *s, int indentLen)
{
    static char pbuf[myBUFSIZ*2];
    char *cp1 = pbuf;
    char *cp2;
    int len;

    if ((len = strlen(s)) > sizeof(pbuf)) {
	Warn("buffer contents \"%s\"", s);
	Fatal("Pretty-print data too long (%d)!", len);
    }
    strcpy(pbuf, s);
    if (indentLen > 10) {
	indentLen = 10;
    } else if (indentLen < 0) {
	indentLen = 0;
    }

    while (isspace(*cp1)) {		/* skip leading white-space */
	cp1++;
    }

    while (strlen(cp1) > _PP_LINESIZE) {
	cp2 = (char *) (cp1+_PP_LINESIZE-1);
	while ((*cp2 != ' ') && (cp2 > cp1)) {
	    cp2--;
	}
	*cp2 = NULL_CHAR;		/* end line */
	fprintf(fp, "%s\n", cp1);	/* dump line */
	*cp2 = ' ';			/* reset it */
	cp1 = cp2 + 1;			/* indented line */
	if (indentLen) {
	    cp1 -= indentLen;
	    strncpy(cp1, "          ", indentLen);
	}
    }
    fprintf(fp, "%s\n", cp1);
    return;
}


/*
 * Construct a 'computed license'.  Wherever possible, leave off the
 * entries for None and LikelyNot; those are individual-file results
 * and we're making an 'aggregate summary' here.
 *****
 * parseLicenses() added license components found, as long as they were
 * considered "interesting" to some extent.  Components of significant
 * interest had their iFlag set to 1; those of lower-interest were set to
 * 0.  In this way we can tier license components into 4 distinct levels:
 * 'interesting', 'medium interest', 'nothing significant', and 'Zero'.
 * ==> If the list is EMPTY, there's nothing, period.
 * ==> If listCount() returns non-zero, "interesting" stuff is in it and
 *     we can safely ignore things of 'significantly less interest'.
 * ==> If neither of these is the case, only the licenses of the above
 *     'significantly less interest' category exist (don't ignore them).
 ******
 * We need to be VERY careful in this routine about the length of the
 * license-summary created; they COULD be indefinitely long!  For now,
 * just check to see if we're going to overrun the buffer...
 */

/**
  \brief Construct a 'computed license'.

  Wherever possible, leave off the entries for None and LikelyNot; those are
  individual-file results and we're making an 'aggregate summary' here.

  This function adds licenses to cur.compLic

 */
static void makeLicenseSummary(list_t *l, int highScore, char *target, int size)
{
    item_t *p;
    int printCount = 0;
    int len = 0;
    int new;
    int goodStuff;

#ifdef	PROC_TRACE
    traceFunc("== makeLicenseSummary(%p, %d, %p, %d)\n", l, highScore,
	      target, size);
#endif	/* PROC_TRACE */

    if (l->used == 0) {		/* zero/nothing */
	(void) strcpy(target, LS_NOSUM);
	return;
    }
    /*
     * Now we know there's something in the list of AT LEAST marginal interest
     * in the component-list.  If listCount() is zero, ALL data is 'marginal';
     * else we have 'good stuff'.  For the latter, we only summarize items with
     * a 'val' > 0 (the really interesting stuff).
     */
    listSort(l, SORT_BY_COUNT_DSC);	/* sort components */
    size--;		/* maximum strlen, adjust to allow *1* NULL */
    for (goodStuff = 0; (p = listIterate(l)) != NULL_ITEM; /*nada */) {
	if (p->iLevel) {
	    goodStuff = 1;		/* interesting license */
	    l->ix = -1;		/* reset saved index */
	    break;
	}
    }
    while ((p = listIterate(l)) != NULL_ITEM) {
	if (goodStuff && (p->iLevel <= IL_LOW)) {	/* uninteresting */
	    continue;
	}
	if (printCount) {
	    target[len++] = ',';
	}
	printCount++;
	new = sprintf(target+len, "%s", p->str);
	if ((len += new) > size) {
	    Fatal("Buffer-overwrite, marginal license components");
	}
	new = 0;
    }
    return;
}


#ifdef	LICENSE_DEBUG
dumpLicenses()
{
    int i;

#ifdef	PROC_TRACE
    traceFunc("== dumpLicenses()\n");
#endif	/* PROC_TRACE */

    for (i = 0; i < NFOOTPRINTS; i++) {
	printf("License[%d]: seedlen=%d, regexlen=%d\n", i,
	       licSpec[i].seed.csLen, licSpec[i].text.csLen);
    }
    printf("[NFOOTPRINTS = %d\n", NFOOTPRINTS);
}
#endif	/* LICENSE_DEBUG */



#ifdef DEAD_CODE
#define	EXCERPT_MAXLEN	128

static void findLines(char *path, char *textp, int tsize, int index,
    list_t *list)
{
    char *para;
    char *cp;
    char *start;
    char *end;
    char buf[myBUFSIZ];
    item_t *p;
#ifdef	PHRASE_DEBUG
    int i;
#endif	/* PHRASE_DEBUG */

#if defined PROC_TRACE || defined PHRASE_DEBUG
    traceFunc("== findLines(%p, %d, %d, %p)\n", textp, tsize, index, list);
#endif	/* PROC_TRACE || PHRASE_DEBUG */

    if (_SEED(index) == NULL_STR) {
	Assert(NO, "Empty (NULL) key for search-regex %d", index);
    }
    para = getInstances(textp, tsize, 0, 0, _SEED(index), NO);
    if (para == NULL_STR) {
	return;
    }
    cp = para;
    /*
     * Search for the line; remember, we might get/find >1 !!
     */
    while (idxGrep(index, cp, REG_ICASE|REG_NEWLINE|REG_EXTENDED)) {
#ifdef	PHRASE_DEBUG
	printf("DEBUG: STR[");
	for (i = cur.regm.rm_so; i < cur.regm.rm_eo; i++) {
	    printf("%c", *(cp+i));
	}
	printf("]...\n");
#endif	/* PHRASE_DEBUG */
	cp += cur.regm.rm_so;
	start = findBol(cp, para);
	if ((end = findEol(cp)) == NULL_STR) {
	    Fatal("Searching for EOL is all for naught!");
	}
	*buf = *end = NULL_CHAR;
#ifdef	PHRASE_DEBUG
	printf("DEBUG: CONTEXT[");
	for (i = 0; i < EXCERPT_MAXLEN && *(cp+i) != NULL_CHAR; i++) {
	    printf("%c", *(cp+i));
	}
	if (*(cp+i) == NULL_CHAR) {
	    printf("<NULL!>");
	}
	printf("]...\n");
	printf("DEBUG: start %p end %p (diff %d)\n", start, end,
	       end-start);
#endif	/* PHRASE_DEBUG */
	/*
	 * If the line we found is 'long', pretty-print an excerpt of the text
	 * surrounding our target string; else, record the whole line.
	 */
	if (end-start > EXCERPT_MAXLEN) {
#ifdef	QA_CHECKS
	    Assert(NO, "findLines: LONG line in %s", path);
#endif	/* QA_CHECKS */
	    (void) strcat(buf, "[long-line-excerpt follows]\n");
	    if (cp+EXCERPT_MAXLEN <= end) {
		*(cp+EXCERPT_MAXLEN) = NULL_CHAR;
	    }
	    (void) strcat(buf, cp);
	    (void) strcat(buf, "\n[end-of-excerpt]");
#ifdef	PHRASE_DEBUG
	    Msg("%d-byte line >> %d (index %d)\n%s\n",
		end-cp, EXCERPT_MAXLEN, index, buf);
#endif /* PHRASE_DEBUG */
	} else {
	    (void) strcpy(buf, start);
	}
#ifdef	CACHE_DEBUG
	printf("findLines: enqueue \"%s\"\n", buf);
#endif	/* CACHE_DEBUG */
	*end = '\n';	/* restore newline */
	cp = end;
	/*
	 * Enqueue this text
	 */
	doctorBuffer(buf, 0, 0, YES);
	p = listGetItem(list, buf);
	p->refCount++;
    }
    return;
}
#endif /* DEAD_CODE */

int findParagraph(char *textp, int size, scanres_t *scp,
    char *pathname, int index, char *label, int exists)
{
    char *buf;
    char header[22];
    FILE *refFp;
    int isNew = 0;
    /* */
#if defined(PROC_TRACE) || defined(PHRASE_DEBUG)
    traceFunc("== findParagraph(%p, %d, %p, %s, %d, %s, %d)\n", textp,
	      size, scp, pathname, index, label, exists);
#endif	/* PROC_TRACE || PHRASE_DEBUG */

#ifdef	PHRASE_DEBUG
    printf("findParagraph: looking for \"%s\" --> %s\n",
	   _REGEX(index), pathname);
#endif	/* PHRASE_DEBUG */
    buf = getInstances(textp, size, gl.uPsize, gl.uPsize, _REGEX(index), YES);
    if (buf == NULL_STR) {
	return(0);
    }

    isNew = !isFILE(pathname);
    refFp = fopen(pathname, "a+");
    if (isNew) {
	fprintf(refFp, "Hotword: %s\n", label);
    }

    (void) snprintf(header, 9+strlen(scp->linkname), "%s", HASHES);
    fprintf(refFp, "%s\n### %s ### %s\n%s\n%s\n", header, scp->linkname,
	    scp->relpath, header, buf);
    (void) fclose(refFp);
    return(1);
}


#ifdef notdef
/*
 * These checks used to be performed on every run, but it makes better
 * sense to run this at compile-time.
 */
/*
  CDB - Appears to be dead code
*/
static void licenseStringChecks()
{
    int i;
    int j;
    char *cp;

#if defined(PROC_TRACE) || defined(PHRASE_DEBUG)
    traceFunc("== licenseStringChecks()\n");
#endif	/* PROC_TRACE || PHRASE_DEBUG */

    /*
     * Look to see if the key (tseed) is a sub-string of the regex
     */
    for (i = 0; i < NFOOTPRINTS; i++) {
	if (_SEED(i) == NULL_STR) {
	    continue;
	}
#if	((DEBUG > 5) || defined LICENSE_DEBUG)
	if (_SEED(i) == _REGEX(i)) {
	    Assert(NO, "Lic %d tseed == regex", i);
	}
#endif	/* (DEBUG>5 || LICENSE_DEBUG) */
	if (!strGrep(_SEED(i), _REGEX(i), REG_EXTENDED|REG_ICASE) &&
	    !strNbuf(_REGEX(i), _SEED(i))) {
	    Assert(NO, "Seed %d [\"%s\"] NOT a regex substring",
		   i, _SEED(i));
	}
    }
    /*
     * Check for duplicate regex-strings (duplicate keys are allowed/encouraged)
     */
    for (i = 0; i < NFOOTPRINTS; i++) {
	for (cp = _REGEX(i); *cp; cp++) {
	    if (*cp == ':' && cp != _REGEX(i) &&
		strncmp(cp-2, "[^:]", 4)) {
		Warn("Regex %d: embedded '%c'", i, *cp);
		break;
	    }
	}
	for (j = i + 1; j < NFOOTPRINTS; j++) {
	    if (licSpec[i].text.csLen != licSpec[j].text.csLen) {
		continue;
	    }
	    if (strcmp(_REGEX(i), _REGEX(j)) == 0) {
		Assert(NO, "Regex %d and %d are identical", i, j);
	    }
	}
	if ((j = regcomp(&cur.regc, _REGEX(i),
			 REG_EXTENDED|REG_NEWLINE|REG_ICASE)) != 0) {
	    Assert(NO, "Regex-compile failed(%d), regex %d", j, i);
	    regexError(j, &cur.regc, _REGEX(i));
	    printf("\"%s\"\n", _REGEX(i));
	}
	regfree(&cur.regc);
    }
    if (!optionIsSet(OPTS_DEBUG)) {
	return;
    }
    for (i = _LT_first; i <= _LT_last; i++) {
	for (j = _LT_first; j <= _LT_last; j++) {
	    if (i == j) {
		continue;
	    }
	    if (idxGrep(i, _REGEX(j), REG_EXTENDED)) {
		printf("=+=+=\n");
		Warn("Regex %d matches regex/text #%d", i, j);
		(void) mySystem("egrep \"	%d$\" _autodefs.h", i);
		printf("  REGEX: %s\n", _REGEX(i));
		(void) mySystem("egrep \"	%d$\" _autodefs.h", j);
		printf("MATCHES: %s\n", _REGEX(j));
	    }
	}
    }
    return;
}
#endif /* notdef */
