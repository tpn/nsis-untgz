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
 */

#ifdef __cplusplus
extern "C" {
#endif

/* mini Standard C library replacement */
#include "miniclib.h"

/* library upon which all this work based on/requires */
#include "zlib/zlib.h"

#define CM_AUTO -1 /* flags during init to auto guess file type */
#define CM_NONE 0  /* no compression */
#define CM_GZ   1  /* gz compressed */
#define CM_LZMA 2  /* lzma compressed */
#define CM_BZ2  3  /* bzip2 compressed */
#define CM_Z    4  /* unsupported, compress compressed */

/* comment out to disable support for unneeded compression methods */
/* NONE and GZ are always enabled */
#define ENABLE_LZMA
#define ENABLE_BZ2
/* #define ENABLE_Z */

/* action to perform when extracting file from tarball */
enum KeepMode {
  OVERWRITE=0, /* default, overwrite file if exists */
  SKIP,        /* if file exists, skip extraction (keep all existing files) */
  UPDATE,      /* if file exists and newer, skip extraction (extract only if archived file is newer) */
};

/* actual extraction routine */
int tgz_extract(gzFile tgzFile, int cm, int junkPaths, enum KeepMode keep, int iCnt, char *iList[], int xCnt, char *xList[]);

/* recursive make directory */
/* abort if you get an ENOENT errno somewhere in the middle */
/* e.g. ignore error "mkdir on existing directory" */
/* */
/* return 1 if OK */
/*        0 on error */
int makedir (char *newdir);


/* tar header */

#define BLOCKSIZE 512
#define SHORTNAMESIZE 100
#define PFXNAMESIZE 155

struct tar_header
{                       /* byte offset */
  char name[100];       /*   0 */
  char mode[8];         /* 100 */
  char uid[8];          /* 108 */
  char gid[8];          /* 116 */
  char size[12];        /* 124 */
  char mtime[12];       /* 136 */
  char chksum[8];       /* 148 */
  char typeflag;        /* 156 */
  char linkname[100];   /* 157 */
  char magic[6];        /* 257 */ /* older gnu tar combines magic+version = 'uname  \0' */
  char version[2];      /* 263 */ /* posix has magic = 'uname\0' and version = '00' */
  char uname[32];       /* 265 */
  char gname[32];       /* 297 */
  char devmajor[8];     /* 329 */
  char devminor[8];     /* 337 */
  char prefix[155];     /* 345 */
  char fill[12];         /* 500 */ /* unused */
};

union tar_buffer {
  char               buffer[BLOCKSIZE];
  struct tar_header  header;
};


/* validate checksum */
/* returns 0 if failed check */
/* returns nonzero if either or signed/unsigned checksum matches */
int valid_checksum(struct tar_header *header);


/* uses filename & file contents and returns best guess of file type CM_* */
int getFileType(const char *fname);


/* !!!USER SUPPLIED!!! */
/* wrap around whatever you want to send error messages to user, c function */
void PrintMessage(const TCHAR *msg, ...);


#ifdef __cplusplus
}
#endif
