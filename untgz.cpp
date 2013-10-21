/*
  untgz - unzip like replacement plugin, except for tarballs
  KJD <jeremyd@computer.org> 2002-2005

  Initial plugin and TAR extraction logic derived from untgz.c
  included with zlib.
  * written by "Pedro A. Aranda Guti\irrez" <paag@tid.es>
  * adaptation to Unix by Jean-loup Gailly <jloup@gzip.org>

  Copyright and license:
  I personally add no additional copyright, so the license is the
  combination of NSIS exDLL (example plugin) and decompression
  libraries used.  For basic gzip'd tarballs (which rely on zlib),
  this is essentially MIT/BSD licensed.  Support for lzma compressed
  tarballs requires the LZMA files and their LGPL/CPL with exception
  license.  Please see the included readme and/or libraries for
  complete copyright and license information.
  
  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors (or copyright holders) be
  held liable for any damages arising from the use of this software.

*/


/*
  USAGE:
  untgz::extract [-j] [-d basedir] [-k|-u] [-z<type>] [-x] [-f] tarball.tgz
    extracts files from tarball.tgz
      if [option] is specified then:
         -j       ignore paths in tarball (junkpaths)
         -d       will extract relative to basedir
         -k is    will not overwrite existing files (keep)
         -u is    will only overwrite older files (update)
         -z is    determines compression used, see below
  untgz::extractV [-j] [-d basedir] [-k|-u] [-z<type>] [-x] [-f] tarball.tgz [-i {iList}] [-x {xList}] --
    extracts files from tarball.tgz
      if [option] is specified then:
         -j       ignore paths in tarball (junkpaths)
         -d       will extract relative to basedir
         -k is    will not overwrite existing files (keep)
         -u is    will only overwrite older files (update)
         -z is    determines compression used, see below
      if -i is specified will only extract files whose filename matches
      if -x is specified will NOT extract files whose filename matches
      the -- is required and marks the end of the file lists
  untgz::extractFile [-d basedir] [-z<type>] tarball.tgz file
    extracts just the file specified
      path information is ignored, implictly -j is specified (may also be explicit)

  For compatibility with tar command, the following option specifiers may be
  used (must appear prior to filename argument), however, they are simply ignored.
      -x indicates action to perform is extraction (extract)
      -f archive-name indicates name of tarball (filename), note
         even when used, the filename must be last argument

  If neither -k or -u is used then all existing files will be replaced
  by corresponding file contained within archive.  

  The -z<type> option may be specified to explicitly indicate how
  tar file is compressed.
  -z      indicates gzip (.tgz/.tar.gz) compression, uses zlib,
  -zgz    alias for -z
  -znone  indicates uncompressed tar file (.tar)
  -zlzma  indicates lzma (.tlz/.tar.lzma) compression
  -zbz2   indicates bzip2 (.tbz/.tar.bz2) compression
  -zZ     indicates compress (.tZ/.tar.Z) compression UNSUPPORTED
  -zauto  determines type based on content & extension
          this is the default if -z<type> option is omitted
          NOTE: prior to version 1.0.15 -z was the default
  

  NOTES:
    Without -j there is a security issue as no checking is done to paths,
    allowing untrusted tarballs to overwrite arbitrary files (e.g. /bin/*).
    Also no checking is done to directory or file names.  In untar.c there
    is a hook so custom versions can modify/strip filepaths prior to opening.
    The -d option is currently implemented by a chdir to indicatd directory
    prior to extraction; future versions may instead prepend to extracted path.
*/


// plugin specific headers
#include "nsisUtils.h"
#include "untar.h"

// standard headers
#include <stdarg.h>  /* va_list, va_start, va_end */


/* The exported API without name mangling */
extern "C" {

__declspec(dllexport) void extract(HWND hwndParent, int string_size, TCHAR *variables, stack_t **stacktop);
__declspec(dllexport) void extractV(HWND hwndParent, int string_size, TCHAR *variables, stack_t **stacktop);
__declspec(dllexport) void extractFile(HWND hwndParent, int string_size, TCHAR *variables, stack_t **stacktop);


/* DLL entry function, needs to be __stdcall, but must be extern "C" for proper decoration (name mangling) */
BOOL WINAPI _DllMainCRTStartup(HANDLE _hModule, DWORD ul_reason_for_call, LPVOID lpReserved);

}


// global variables
HINSTANCE g_hInstance;

HWND g_hwndParent;
HWND g_hwndList;


// DLL entry point
BOOL WINAPI _DllMainCRTStartup(HANDLE _hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
  g_hInstance=(HINSTANCE)_hModule;
  mCRTinit();	/* init out mini clib, mostly just stdin/stdout/stderr */
  return TRUE;
}


// macro sets R0 to exit status, "success" or error message
#define setExitStatus(status) setuservariable(INST_R0, status)

// macro to display error status, and sets error value
#define setErrorStatus(status)	\
{	\
    PrintMessage(status);	\
    setExitStatus(status);	\
}

// macro to display command line, sets error value, & then returns
#define exitWithError(status, optSvar)	\
{	\
    PrintMessage(cmdline);	\
    PrintMessage(_T("%s %s"), status, optSvar);	\
    setExitStatus(status);	\
    return;	\
}


// macro that attempts to pop argument off stack
// if one is not available will display (via LogMessage)
// current command line followed by msg (which should
// be a description of the error regarding the
// expected argument that is missing) ...
// poparg also closes tgzFile, else same as poparg1
#define poparg1(buf, msg) { if (popstring(buf)) exitWithError(ERR_NO_TARBALL, _T("")); }
#define poparg(buf, msg) \
{ \
  if (popstring(buf)) \
  { \
    if (tgzFile) gzclose(tgzFile); \
    exitWithError(msg, _T("")) \
  } \
}


// macro that compares argument, if match prints to log and sets variable to value
#define setOpt(arg, var, value) \
  else if (_tcscmp(buf, arg) == 0) \
  { \
    _tcscat(cmdline, buf); \
    _tcscat(cmdline, _T(" ")); \
    *var = value ; \
  }


// error messages
#define ERR_SUCCESS _T("success")  /* DO NOT CHANGE */
#define ERR_OPEN_FAILED _T("Error: Could not open tarball.")
#define ERR_READ _T("Error: Failure reading from tarball.")
#define ERR_EXTRACT _T("Error: Unable to extract file.")
#define MESG_DONE _T("extraction complete.")

#define ERR_NO_TARBALL _T("Error: tarball not specified.")
#define ERR_DOPT_MISSING_DIR _T("Error: -d option given but base directory not specified!")
#define ERR_BAD_INCLUDE_LIST _T("Error: -i unable to obtain include file list!")
#define ERR_BAD_EXCLUDE_LIST _T("Error: -x unable to obtain exclude file list!")
#define ERR_MISSING_INCLUDE_EXCLUDE_TERMINATOR _T("Error: -- include/exclude end marker is missing!")
#define ERR_MISSING_FILE _T("Error: file to extract not specified!")
#define ERR_UNSUPPORTED_COMPRESSION _T("Error: Unsupported compression format.")
#define ERR_UNKNOWN_OPTION _T("Error: unknown option specified!")
#define WARN_INVALID_OPTION _T("WARNING: invalid option (%s), ignoring!")


void argParse(HWND hwndParent, int string_size, TCHAR *variables, stack_t **stacktop, 
              TCHAR *cmd, TCHAR *cmdline, gzFile *tgzFile, int *compressionMethod,
              int *junkPaths, enum KeepMode *keep, TCHAR *basePath)
{
  TCHAR buf[1024];     /* used for argument processor or other temp buffer */
  TCHAR iPath[1024];   /* initial (base) directory for extraction */

  /* setup stack and other general NSIS plugin stuff */
  pluginInit(hwndParent, string_size, variables, stacktop);

  /* initialize our logmessage with command called */
  _tcscpy(cmdline, _T("untgz::"));
  _tcscat(cmdline, cmd);  /* e.g. "extract", "extractV", or "extractFile" */
  _tcscat(cmdline, _T(" "));

  /* usage: untgz::extract* [-j] [-d basedir] tarball.tgz ...other arguments... */

  /* sets INST_R0 to "success" if all goes well, else will be set to an error msg */
  setExitStatus(ERR_SUCCESS);

  /* initialize optional arguments to their general defaults */
  *tgzFile = 0;
  *compressionMethod = CM_AUTO;
  *keep = OVERWRITE;
  *junkPaths = 0;       /* keep path information by default    */
  if (basePath != NULL)
    *basePath = '\0';   /* default to current directory ""     */
  *iPath = '\0';        /* default to current directory ""     */


  /* get 1st optional argument or the tarball itself */
  poparg1(buf, ERR_NO_TARBALL);


  /* cycle through handling options */
  while (*buf == static_cast<TCHAR>('-'))
  {
    if (_tcscmp(buf, _T("-d")) == 0)       /* see if optional basedir specified */
    {
      _tcscat(cmdline, _T("-d "));
      poparg1(buf, ERR_DOPT_MISSING_DIR);

      /* copy over base directory to our logmesage */
      _tcscat(cmdline, _T("'"));
      _tcscat(cmdline, buf);
      _tcscat(cmdline, _T("' "));

	  /* if basepath is not NULL then copy it over for callee */
	  if (basePath != NULL) _tcscpy(basePath, buf);

      /* store so we can set as current directory after opening tarball */
      _tcscpy(iPath, buf);
    }
    setOpt(_T("-j"), junkPaths, 1)  /* see if optional junkpaths specified */
    setOpt(_T("-k"), keep, SKIP)    /* see if no overwrite mode given */
    setOpt(_T("-u"), keep, UPDATE)  /* see if update mode given */
    setOpt(_T("-z"),     compressionMethod, CM_GZ)    /* compression gzipped */
    setOpt(_T("-zgz"),   compressionMethod, CM_GZ)    /* compression gzipped */
    setOpt(_T("-znone"), compressionMethod, CM_NONE)  /* no compression, plain tar */
    setOpt(_T("-zlzma"), compressionMethod, CM_LZMA)  /* compression lzma */
    setOpt(_T("-zbz2"),  compressionMethod, CM_BZ2)   /* compression bz2 */
    setOpt(_T("-zZ"),    compressionMethod, CM_Z)     /* compression compress */
    setOpt(_T("-zauto"), compressionMethod, CM_AUTO)  /* compression to be determined */
    else if ((_tcscmp(buf,_T("-x"))==0)||(_tcscmp(buf,_T("-f"))==0))  /* ignored options */
    {
      /* update our logmessage */
      _tcscat(cmdline, buf);
      _tcscat(cmdline, _T(" "));
    }
    else                              /* else invalid optional argument specified */
    {
      _tcscat(cmdline, _T("<<"));
      _tcscat(cmdline, buf);
      _tcscat(cmdline, _T(">>"));
	  PrintMessage(WARN_INVALID_OPTION, buf);
      //exitWithError(ERR_UNKNOWN_OPTION);
    }

    /* get next optional argument or the tarball itself */
    poparg1(buf, ERR_NO_TARBALL);
  }

  /* copy over tarball file name to our logmessage */
  _tcscat(cmdline, _T("'"));
  _tcscat(cmdline, buf);  
  _tcscat(cmdline, _T("' "));

  /* if auto type specified then determine type */
  if (*compressionMethod == CM_AUTO)
    *compressionMethod = getFileType(_T2A(buf));

  /* check if compression method requested is supported */
  if ((*compressionMethod == CM_Z)
#ifndef ENABLE_BZ2
      || (*compressionMethod == CM_BZ2)
#endif
#ifndef ENABLE_LZMA
      || (*compressionMethod == CM_LZMA)
#endif
     )
  {
    exitWithError(ERR_UNSUPPORTED_COMPRESSION, buf);
  }

  /* PrintMessage("Compression Method is %i", *compressionMethod); */

  /* open tarball so can read/decompress it */
  if ((*tgzFile = gzopen(_T2A(buf),"rb")) == NULL)
    exitWithError(ERR_OPEN_FAILED, buf);

  /* set working dir (after opening tarball) to base
     directory user specified (or leave as current),
     but 1st try to create if it doesn't exist yet.
  */
  if (*iPath) /* != '\0' if not specified, ie current */
  {
    makedir(_T2A(iPath));
    SetCurrentDirectory(iPath);
  }
}


/* indicates method used to determine files to extract from archive */
enum ExtractMode {
  EXTRACT_ALL = 0, /* extract()     */
  EXTRACT_LISTS,   /* extractV()    */
  EXTRACT_SINGLE   /* extractFile() */
};
TCHAR * funcName[] = { _T("extract"), _T("extractV"), _T("extractFile") };

/* performs extraction, where mode indicates what files to extract
   as determined by exported function and its arguments
 */
void doExtraction(enum ExtractMode mode, HWND hwndParent, int string_size, TCHAR *variables, stack_t **stacktop)
{
  register int result;
  TCHAR cmdline[1024];     /* just used to display to user */
  int junkPaths;          /* default to extracting with paths -- highly insecure */
  int compressionMethod;  /* gzip or other compressed tar file */
  enum KeepMode keep;     /* overwrite mode */
  gzFile tgzFile = NULL;  /* the opened tarball (assuming argParse returns successfully) */

  TCHAR buf[1024];         /* used for argument processor or other temp buffer */
  int iCnt=0, xCnt=0;     /* count for elements in list */
  char **iList=NULL,      /* (char *) list[Cnt] for list of files to extract */
       **xList=NULL;      /* (char *) list[Cnt] for list of files to NOT extract */

  /* do common stuff including parsing arguments up to filename to extract */
  argParse(hwndParent, string_size, variables, stacktop, 
           funcName[mode], cmdline, &tgzFile, &compressionMethod, &junkPaths, &keep, NULL);

  /* check if everything up to now processed ok, exit if not */
  if (_tcscmp(getuservariable(INST_R0), ERR_SUCCESS) != 0) return;

  /* if ExtractV used, ie variable file lists, then pop lists off stack */
  if (mode == EXTRACT_LISTS)
  {

  /* get next optional argument or end marker */
  poparg(buf, ERR_MISSING_INCLUDE_EXCLUDE_TERMINATOR);

  while ((*buf == static_cast<TCHAR>('-')) && (_tcscmp(buf, _T("--")) != 0))
  {
    if (_tcscmp(buf, _T("-i")) == 0)
	{
      _tcscat(cmdline, _T("-i "));

      /* get include file list */
      if (getArgList(&iCnt, &iList, cmdline) != 0)
        exitWithError(ERR_BAD_INCLUDE_LIST, "");
	}
	else if (_tcscmp(buf, _T("-x")) == 0)
    {
      _tcscat(cmdline, _T("-x "));

      /* get exclude file list */
      if (getArgList(&xCnt, &xList, cmdline) != 0)
        exitWithError(ERR_BAD_EXCLUDE_LIST, "");
    }
	else                              /* else invalid optional argument specified */
	{
      _tcscat(cmdline, _T("<<"));
      _tcscat(cmdline, buf);
      _tcscat(cmdline, _T(">>"));
	  PrintMessage(WARN_INVALID_OPTION, buf);
      //exitWithError(ERR_UNKNOWN_OPTION);
	}

    /* get next optional argument or end marker */
    poparg(buf, ERR_MISSING_INCLUDE_EXCLUDE_TERMINATOR);
  }

  if (_tcscmp(buf, _T("--")) == 0)
	  _tcscat(cmdline, _T("--"));
  else
	  _tcscat(cmdline, _T("?--?"));

  } /* if (mode == EXTRACT_LISTS) */
  else if (mode == EXTRACT_SINGLE)
  {
    /* actually get the file to extract */
    poparg(buf /*filename*/, ERR_MISSING_FILE);

    /* copy over file name to extract to our logmessage */
    _tcscat(cmdline, _T("'"));
    _tcscat(cmdline, buf);
    _tcscat(cmdline, _T("'"));


    junkPaths = 1;

    iCnt = 1;
    iList = (char **)malloc(sizeof(char *));
    /* if (iList == NULL) return -1; /* error allocating needed memory */
    iList[0] = (char *)malloc(strlen(_T2A(buf))+1);
    strcpy(iList[0], _T2A(buf));  /*filename*/
  }
  /* else if (mode == EXTRACT_ALL) {} */


  /* show user cmdline */
  PrintMessage(cmdline);

  /* actually perform the extraction */
  if ((result = tgz_extract(tgzFile, compressionMethod, junkPaths, keep, iCnt, iList, xCnt, xList)) < 0)
    setErrorStatus((result==-1)?ERR_READ:ERR_EXTRACT)
  else
    PrintMessage(MESG_DONE);

  /* clean up */
  {
    register int i;
    for (i = 0; i < iCnt; i++)  if (iList[i] != NULL) free(iList[i]);
    if (iList != NULL) free(iList);
    for (i = 0; i < xCnt; i++)  if (xList[i] != NULL) free(xList[i]);
    if (xList != NULL) free(xList);
  }
}



/* Implemenation of exported API */

void extract(HWND hwndParent, int string_size, TCHAR *variables, stack_t **stacktop)
{
  doExtraction(EXTRACT_ALL, hwndParent, string_size, variables, stacktop);
}

void extractV(HWND hwndParent, int string_size, TCHAR *variables, stack_t **stacktop)
{
  doExtraction(EXTRACT_LISTS, hwndParent, string_size, variables, stacktop);
}

void extractFile(HWND hwndParent, int string_size, TCHAR *variables, stack_t **stacktop)
{
  doExtraction(EXTRACT_SINGLE, hwndParent, string_size, variables, stacktop);
}


