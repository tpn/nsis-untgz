/*
 * tgz_extract functions based on code within zlib library
 * No additional copyright added, KJD <jeremyd@computer.org>
 *
 *   This software is provided 'as-is', without any express or implied
 *   warranty.  In no event will the authors be held liable for any damages
 *   arising from the use of this software.
 *
 * untgz.c -- Display contents and/or extract file from
 * a gzip'd TAR file
 * written by "Pedro A. Aranda Guti\irrez" <paag@tid.es>
 * adaptation to Unix by Jean-loup Gailly <jloup@gzip.org>
 * various fixes by Cosmin Truta <cosmint@cs.ubbcluj.ro>
*/

/*
  For tar format see
  http://cdrecord.berlios.de/old/private/man/star/star.4.html
  http://www.mkssoftware.com/docs/man4/tar.4.asp 
  http://www.delorie.com/gnu/docs/tar/tar_toc.html

  TODO:
    without -j there is a security issue as no checking is done to directories
    change to better support -d option, presently we just chdir there
*/


#include "untar.h"


/** the rest heavily based on (ie mostly) untgz.c from zlib **/

/* Values used in typeflag field.  */

#define REGTYPE  '0'		/* regular file */
#define AREGTYPE '\0'		/* regular file */
#define LNKTYPE  '1'		/* link */
#define SYMTYPE  '2'		/* reserved */
#define CHRTYPE  '3'		/* character special */
#define BLKTYPE  '4'		/* block special */
#define DIRTYPE  '5'		/* directory */
#define FIFOTYPE '6'		/* FIFO special */
#define CONTTYPE '7'		/* reserved, for compatibility with gnu tar,
                               treat as regular file, where it represents
                               a regular file, but saved contiguously on disk */

/* GNU tar extensions */

#define GNUTYPE_DUMPDIR  'D'    /* file names from dumped directory */
#define GNUTYPE_LONGLINK 'K'    /* long link name */
#define GNUTYPE_LONGNAME 'L'    /* long file name */
#define GNUTYPE_MULTIVOL 'M'    /* continuation of file from another volume */
#define GNUTYPE_NAMES    'N'    /* file name that does not fit into main hdr */
#define GNUTYPE_SPARSE   'S'    /* sparse file */
#define GNUTYPE_VOLHDR   'V'    /* tape/volume header */



/* help functions */

unsigned long getoct(char *p,int width)
{
  unsigned long result = 0;
  char c;
  
  while (width --)
    {
      c = *p++;
      if (c == ' ') /* ignore padding */
        continue;
      if (c == 0)   /* ignore padding, but also marks end of string */
        break;
      if (c < '0' || c > '7')
        return result; /* really an error, but we just ignore invalid values */
      result = result * 8 + (c - '0');
    }
  return result;
}

/* regular expression matching */

#define ISSPECIAL(c) (((c) == '*') || ((c) == '/'))

int ExprMatch(char *string,char *expr)
{
  while (1)
    {
      if (ISSPECIAL(*expr))
	{
	  if (*expr == '/')
	    {
	      if (*string != '\\' && *string != '/')
		return 0;
	      string ++; expr++;
	    }
	  else if (*expr == '*')
	    {
	      if (*expr ++ == 0)
		return 1;
	      while (*++string != *expr)
		if (*string == 0)
		  return 0;
	    }
	}
      else
	{
	  if (*string != *expr)
	    return 0;
	  if (*expr++ == 0)
	    return 1;
	  string++;
	}
    }
}


/* returns 0 on failed checksum, nonzero if probably ok 
   it was noted that some versions of tar compute
   signed chksums, though unsigned appears to be the
   standard; chksum is simple sum of all bytes in header
   as integers (using at least 17 bits) with chksum
   values treated as ASCII spaces.
*/
int valid_checksum(struct tar_header *header)
{
  unsigned hdrchksum = (unsigned)getoct(header->chksum,8);
  signed schksum = 0;
  unsigned uchksum = 0;
  int i;

  for (i=0; i < sizeof(struct tar_header); i++)
  {
    unsigned char val = ((unsigned char *)header)[i];
    if ((i >= 148) && (i < 156)) /* chksum */
    {
      val = ' ';
    }
    schksum += (signed char)val;
    uchksum += val;
  }

  if (hdrchksum == uchksum) return 1;
  if ((int)hdrchksum == schksum) return 2;
  return 0;
}


/* recursive make directory */
/* abort if you get an ENOENT errno somewhere in the middle */
/* e.g. ignore error "mkdir on existing directory" */
/* */
/* return 1 if OK */
/*        0 on error */

int makedir (char *newdir)
{
  char *buffer = strdup(newdir);
  char *p;
  int  len = strlen(buffer);
  
  if (len <= 0) {
    free(buffer);
    return 0;
  }
  if (buffer[len-1] == '/') {
    buffer[len-1] = '\0';
  }
  if (CreateDirectoryA(buffer, NULL) != 0)
    {
      free(buffer);
      return 1;
    }

  p = buffer+1;
  while (1)
    {
      char hold;
      
      while(*p && *p != '\\' && *p != '/')
        p++;
      hold = *p;
      *p = 0;
      //if ((mkdir(buffer, 0775) == -1) && (errno == ENOENT /* != EEXIST */))
      if (!CreateDirectoryA(buffer, NULL) && !((GetLastError()==ERROR_FILE_EXISTS) || (GetLastError()==ERROR_ALREADY_EXISTS)) )
      {
        // fprintf(stderr,"Unable to create directory %s\n", buffer);
        PrintMessage(_T("Unable to create directory %s\n"), _A2T(buffer));
        free(buffer);
	  return 0;
      }
      if (hold == 0)
        break;
      *p++ = hold;
    }
  free(buffer);
  return 1;
}

/* NOTE: This should be modified to perform whatever steps
   deemed necessary to make embedded paths safe prior to
   creating directory or file of given [path]filename.
   Must modify fname in place, always leaving either
   same or smaller strlen than current string.
   Current version (if not #defined out) removes any
   leading parent (..) or root (/)(\) references.
*/
void safetyStrip(char * fname)
{
#if 0
  /* strip root from path */
  if ((*fname == '/') || (*fname == '\\'))
  {
    MoveMemory(fname, fname+1, strlen(fname+1) + 1 );
  }

  /* now strip leading ../ */
  while ((*fname == '.') && (*(fname+1) == '.') && ((*(fname+2) == '/') || (*(fname+2) == '\\')) )
  {
    MoveMemory(fname, fname+3, strlen(fname+3) + 1 );
  }
#endif
}


/* combines elements from tar header to produce
 * full [long] filename; prefix + [/] + name
 */
void getFullName(union tar_buffer *buffer, char *fname)
{
	int len = 0;

	/* NOTE: prepend buffer.head.prefix if tar archive expected to have it */
	if (*(buffer->header.prefix) && (*(buffer->header.prefix) != ' '))
	{
		/* copy over prefix */
		strncpy(fname,buffer->header.prefix, sizeof(buffer->header.prefix));
		fname[sizeof(buffer->header.prefix)-1] = '\0';
		/* ensure ends in dir separator, implied after if full prefix size used */
		len = strlen(fname)-1; /* assumed by test above at least 1 character */
		if ((fname[len]!='/') && (fname[len]!='\\'))
		{
			len++;
			fname[len]='/';
		}
		len++; /* index of 1st character after dir separator */
	}

	/* copy over filename portion */
	strncpy(fname+len,buffer->header.name, sizeof(buffer->header.name));
	fname[len+sizeof(buffer->header.name)-1] = '\0'; /* ensure terminated */
}


/* returns a pointer to a static buffer
 * containing fname after removing all but
 * path_sep_cnt path separators
 * if there are less than path_sep_cnt
 * separators then all will still be there.
 */
char * stripPath(int path_sep_cnt, char *fname)
{
  static char buffer[1024];
  char *fname_use = fname + strlen(fname);
  register int i=path_sep_cnt;
  do
  {
    if ( (*fname_use == '/') || (*fname_use == '\\') ) 
	{ 
      i--;
	  if (i < 0) fname_use++;
	  else fname_use--;
    }
	else
      fname_use--;
  } while ((i >= 0) && (fname_use > fname));
  
  strcpy(buffer, fname_use);
  return buffer;
}

/* returns 1 if fname in list else return 0 
 * returns 0 if list is NULL or cnt is < 0
 */
int matchname (char *fname, int cnt, char *list[], int junkPaths)
{
  register char *t;
  int i;
  int path_sep;

  /* if nothing to compare with then return failure */
  if ((list == NULL) || (cnt <= 0))
    return 0;

  for (i = 0; i < cnt; i++)
  {
    /* get count of path components in current filelist entry */
    path_sep = 0;
    if (!junkPaths)
	{
      for(t = list[i]; *t != '\0'; t++)
        if ((*t == '/') || (*t == '\\'))
          path_sep++;
	}
    if (ExprMatch(stripPath(path_sep, fname), list[i]))
      return 1;
  }

  return 0; /* no match */
}


typedef unsigned long time_t;

#ifdef __GNUC__
#define HUNDREDSECINTERVAL 116444772000000000LL
#else
#define HUNDREDSECINTERVAL 116444772000000000i64
#endif
void cnv_tar2win_time(time_t tartime, FILETIME *ftm)
{
#ifdef HAS_LIBC_CAL_FUNCS
		  FILETIME ftLocal;
		  SYSTEMTIME st;
		  struct tm localt;
 
		  localt = *localtime(&tartime);
		  
		  st.wYear = (WORD)localt.tm_year+1900;
		  st.wMonth = (WORD)localt.tm_mon+1;    /* 1 based, not 0 based */
		  st.wDayOfWeek = (WORD)localt.tm_wday;
		  st.wDay = (WORD)localt.tm_mday;
		  st.wHour = (WORD)localt.tm_hour;
		  st.wMinute = (WORD)localt.tm_min;
		  st.wSecond = (WORD)localt.tm_sec;
		  st.wMilliseconds = 0;
		  SystemTimeToFileTime(&st,&ftLocal);
		  LocalFileTimeToFileTime(&ftLocal,ftm);
#else
	// avoid casts further below
    LONGLONG *t = (LONGLONG *)ftm;

	// tartime == number of seconds since midnight Jan 1 1970 (00:00:00)
	// convert to equivalent 100 nanosecond intervals
	*t = UInt32x32To64(tartime, 10000000UL);

	// now base on 1601, add number of 100 nansecond intervals between 1601 & 1970
	*t += HUNDREDSECINTERVAL;  /* 116444736000000000i64; */
#endif
}


#ifdef ENABLE_LZMA
#include "lzma/lzma.h"
LZMAFile *lzmaFile;
#endif
#ifdef ENABLE_BZ2
#include "bz2/bz2.h"
static int bzerror;
BZFILE *bzfile;
void bz_internal_error ( int errcode ) { PrintMessage(_T("BZ2: internal error decompressing!")); }
#endif
gzFile infile;

/* Initialize decompression library (if needed)
   0=success, nonzero means error during initialization
 */
int cm_init(gzFile in, int cm)
{
  infile = in; /* save gzFile for reading/cleanup */

  switch (cm)
  {
#ifdef ENABLE_BZ2
    case CM_BZ2:
      bzfile = BZ2_bzReadOpen(&bzerror, in, 0, 0, NULL, 0);
	return bzerror;
#endif
#ifdef ENABLE_LZMA
    case CM_LZMA:
      return lzma_init(in, &lzmaFile);
#endif
    default: /* CM_NONE, CM_GZ */
      return 0; /* success */
  }
}


/* properly cleanup any resources decompression library allocated 
 */
void cm_cleanup(int cm)
{
  switch (cm)
  {
#ifdef ENABLE_BZ2
    case CM_BZ2:
      BZ2_bzReadClose(&bzerror, bzfile);
      break;
#endif
#ifdef ENABLE_LZMA
    case CM_LZMA:
      lzma_cleanup(lzmaFile);
      break;
#endif
    default: /* CM_NONE, CM_GZ */
      break;
  }

  /* close the input stream */
  if (gzclose(infile) != Z_OK)
  {
    PrintMessage(_T("failed gzclose"));
    /* return -1; */
  }
}


/* Reads in a single TAR block
 */
long readBlock(int cm, void *buffer)
{
  long len = -1;
  switch (cm)
  {
#ifdef ENABLE_BZ2
    case CM_BZ2:
	len = BZ2_bzRead(&bzerror, bzfile, buffer, BLOCKSIZE);
      break;
#endif
#ifdef ENABLE_LZMA
    case CM_LZMA:
      len = lzma_read(lzmaFile, buffer, BLOCKSIZE);
      break;
#endif
    default: /* CM_NONE, CM_GZ */
      len = gzread(infile, buffer, BLOCKSIZE);
      break;
  }

  /* check for read errors and abort */
  if (len < 0)
  {
    PrintMessage(_T("gzread: error decompressing"));
    cm_cleanup(cm);
    return -1;
  }
  /*
   * Always expect complete blocks to process
   * the tar information.
   */
  if (len != BLOCKSIZE)
  {
    PrintMessage(_T("gzread: incomplete block read"));
    cm_cleanup(cm);
    return -1;
  }

  return len; /* success */
}


/* Tar file extraction
 * gzFile in, handle of input tarball opened with gzopen
 * int cm, compressionMethod
 * int junkPaths, nonzero indicates to ignore stored path (don't create directories)
 * enum KeepMode keep, indicates to perform if file exists
 * int iCnt, char *iList[], argv style list of files to extract, {0,NULL} for all
 * int xCnt, char *xList[], argv style list of files NOT to extract, {0,NULL} for none
 *
 * returns 0 (or positive value) on success
 * returns negative value on error, where
 *   -1 means error reading from tarball
 *   -2 means error extracting file from tarball
 */
int tgz_extract(gzFile in, int cm, int junkPaths, enum KeepMode keep, int iCnt, char *iList[], int xCnt, char *xList[])
{
  int           getheader = 1;    /* assume initial input has a tar header */
  HANDLE        outfile = INVALID_HANDLE_VALUE;

  union         tar_buffer buffer;
  unsigned long remaining;
  char          fname[BLOCKSIZE]; /* must be >= BLOCKSIZE bytes */
  time_t        tartime;

  /* do any prep work for extracting from compressed TAR file */
  if (cm_init(in, cm))
  {
    PrintMessage(_T("tgz_extract: unable to initialize decompression method."));
    cm_cleanup(cm);
    return -1;
  }
  
  while (1)
  {
    if (readBlock(cm, &buffer) < 0) return -1;
      
    /*
     * If we have to get a tar header
     */
    if (getheader >= 1)
    {
      /*
       * if we met the end of the tar
       * or the end-of-tar block,
       * we are done
       */
      if (/* (len == 0)  || */ (buffer.header.name[0]== 0)) break;

      /* compute and check header checksum, support signed or unsigned */
      if (!valid_checksum(&(buffer.header)))
      {
        PrintMessage(_T("tgz_extract: bad header checksum"));
        cm_cleanup(cm);
        return -1;
      }

      /* store time, so we can set the timestamp on files */
      tartime = (time_t)getoct(buffer.header.mtime,12);

      /* copy over filename chunk from header, avoiding overruns */
      if (getheader == 1) /* use normal (short or posix long) filename from header */
      {
        /* NOTE: prepends any prefix, including separator, and ensures terminated */
		getFullName(&buffer, fname);
      }
      else /* use (GNU) long filename that preceeded this header */
      {
#if 0
        /* if (strncmp(fname,buffer.header.name,SHORTNAMESIZE-1) != 0) */
        char fs[SHORTNAMESIZE];   /* force strings to same max len, then compare */
        lstrcpyn(fs, fname, SHORTNAMESIZE);
        fs[SHORTNAMESIZE-1] = '\0';
        buffer.header.name[SHORTNAMESIZE-1] = '\0';
        if (lstrcmp(fs, buffer.header.name) != 0)
        {
          PrintMessage(_T("tgz_extract: mismatched long filename"));
          cm_cleanup(cm);
          return -1;
        }
#else
		PrintMessage(_T("tgz_extract: using GNU long filename [%s]"), _A2T(fname));
#endif
      }
      /* LogMessage("buffer.header.name is:");  LogMessage(fname); */


      switch (buffer.header.typeflag)
      {
        case DIRTYPE:
            dirEntry:
            if (!junkPaths)
            {
               safetyStrip(fname);
               makedir(fname);
            }
	      break;
		case CONTTYPE:  /* contiguous file, for compatibility treat as normal */
        case REGTYPE:
        case AREGTYPE:
	      /* Note: a file ending with a / may actually be a BSD tar directory entry */
	      if (fname[strlen(fname)-1] == '/')
	        goto dirEntry;

	      remaining = getoct(buffer.header.size,12);
	      if ( /* add (remaining > 0) && to ignore 0 zero byte files */
               ( (iList == NULL) || (matchname(fname, iCnt, iList, junkPaths)) ) &&
               (!matchname(fname, xCnt, xList, junkPaths))
             )
	      {
			  if (!junkPaths) /* if we want to use paths as stored */
			  {
	              /* try creating directory */
	              char *p = strrchr(fname, '/');
	              if (p != NULL) 
	              {
	                *p = '\0';
	                makedir(fname);
	                *p = '/';
	              }
			  }
			  else
			  {
	              /* try ignoring directory */
	              char *p = strrchr(fname, '/');
	              if (p != NULL) 
	              {
	                /* be sure terminating '\0' is copied and */
	                /* use ansi memcpy equivalent that handles overlapping regions */
	                MoveMemory(fname, p+1, strlen(p+1) + 1 );
	              }
	          }
	          if (*fname) /* if after stripping path a fname still exists */
	          {
	            /* Attempt to open the output file and report action taken to user */
	            const TCHAR szERRMsg[] = _T("Error: Could not create file "),
	                        szSUCMsg[] = _T("Writing "),
	                        szSKPMsg[] = _T("Skipping ");
	            const TCHAR * szMsg = szSUCMsg;

	            safetyStrip(fname);

	            /* Open the file for writing mode, creating if doesn't exist and truncating if exists and overwrite mode */
	            outfile = CreateFileA(fname,GENERIC_WRITE,FILE_SHARE_READ,NULL,(keep==OVERWRITE)?CREATE_ALWAYS:CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);

	            /* failed to open file, either valid error (like open) or it already exists and in a keep mode */
	            if (outfile == INVALID_HANDLE_VALUE)
	            {
	              /* if skip existing or only update existing and failed to open becauses exists */
	              if ((keep!=OVERWRITE) && (GetLastError()==ERROR_FILE_EXISTS))
	              {
	                /* assume skipping initially (mode==SKIP or ==UPDATE with existing file newer) */
	                szMsg = szSKPMsg; /* and update output message accordingly */

					/* if in update mode, check filetimes and reopen in overwrite mode */
	                if (keep == UPDATE)
	                {
	                  FILETIME ftm_a;
                      HANDLE h;
                      WIN32_FIND_DATAA ffData;
 
	                  cnv_tar2win_time(tartime, &ftm_a); /* archive file time */
	                  h = FindFirstFileA(fname, &ffData); /* existing file time */

                      if (h!=INVALID_HANDLE_VALUE)
                        FindClose(h);  /* cleanup search handle */
                      else
                        goto ERR_OPENING;

                      /* compare date+times, is one in tarball newer? */
                      if (*((LONGLONG *)&ftm_a) > *((LONGLONG *)&(ffData.ftLastWriteTime)))
                      {
                        outfile = CreateFileA(fname,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
                        if (outfile == INVALID_HANDLE_VALUE) goto ERR_OPENING;
                        szMsg = szSUCMsg;
                      }
	                }
	              }
	              else /* in overwrite mode or failed for some other error than exists */
	              {
                    ERR_OPENING:
	                PrintMessage(_T("%s%s [%d]"), szERRMsg, _A2T(fname), GetLastError());
	                cm_cleanup(cm);
	                return -2;
	              }
	            }

 	            /* Inform user of current extraction action (writing, skipping file XYZ) */
	            PrintMessage(_T("%s%s"), szMsg, _A2T(fname));
	          }
	      }
	      else
	          outfile = INVALID_HANDLE_VALUE;

	      /*
	       * could have no contents, in which case we close the file and set the times
	       */
	      if (remaining > 0)
	          getheader = 0;
		  else
	      {
	          getheader = 1;
	          if (outfile != INVALID_HANDLE_VALUE)
	          {
	              FILETIME ftm;
 
	              cnv_tar2win_time(tartime, &ftm);
	              SetFileTime(outfile,&ftm,NULL,&ftm);
	              CloseHandle(outfile);
	              outfile = INVALID_HANDLE_VALUE;
	          }
	      }

	      break;
		case GNUTYPE_LONGLINK:
		case GNUTYPE_LONGNAME:
		{
	      remaining = getoct(buffer.header.size,12);
	      if (readBlock(cm, fname) < 0) return -1;
	      fname[BLOCKSIZE-1] = '\0';
	      if ((remaining >= BLOCKSIZE) || ((unsigned)strlen(fname) > remaining))
	      {
	          PrintMessage(_T("tgz_extract: invalid long name"));
	          cm_cleanup(cm);
	          return -1;
	      }
	      getheader = 2;
	      break;
		}
        default:
/*
	      if (action == TGZ_LIST)
	          printf(" %s     <---> %s\n",strtime(&tartime),fname);
*/
	      break;
      }
    }
    else  /* (getheader == 0) */
    {
      unsigned int bytes = (remaining > BLOCKSIZE) ? BLOCKSIZE : remaining;
	  unsigned long bwritten;

      if (outfile != INVALID_HANDLE_VALUE)
      {
          WriteFile(outfile,buffer.buffer,bytes,&bwritten,NULL);
		  if (bwritten != bytes)
          {
			  PrintMessage(_T("Error: write failed for %s"), _A2T(fname));
              CloseHandle(outfile);
              DeleteFileA(fname);

              cm_cleanup(cm);
              return -2;
          }
      }
      remaining -= bytes;
      if (remaining == 0)
      {
          getheader = 1;
          if (outfile != INVALID_HANDLE_VALUE)
          {
              FILETIME ftm;
 
              cnv_tar2win_time(tartime, &ftm);
              SetFileTime(outfile,&ftm,NULL,&ftm);
              CloseHandle(outfile);
              outfile = INVALID_HANDLE_VALUE;
          }
      }
    }
  } /* while(1) */
  
  cm_cleanup(cm);

  return 0;
}

