#include "untar.h"   /* for compression types, CM_* */

/* returns one of CM_* values to indicate compression type,
   gnu/posix tar, gz, bz2, & z have marker bytes, lzma & old tar don't
   if any error returns CM_GZ
   if size > sizeof(tar header), 1st compute header chksum
     and if matches chksum stored in header then assume tar (CM_NONE)
   if file starts with "\037\0213" then assume gzipped (CM_GZ)
   if file starts with "\037\0235" then assume compressed (CM_Z) [UNSUPPORTED]
   if file starts with "BZh" then assume bz2 (CM_BZ2) ("BZ"=bzip + version "h"=2)
   if file extension .tgz or .gz then assume (CM_GZ)
   if file extension .tbz or .bz2 then assume (CM_BZ2)
   if file extension .lzma or .tlz then assume lzma (CM_LZMA)
   otherwise if 1st byte valid PROPERTY return CM_LZMA else CM_GZ
*/

int getFileType(const char *fname)
{
    /* read in chunk of data and try to make determination */
    union tar_buffer buf;
    FILE *f;
    const char *fext;
    
    /* point to extension, whatever after last . */
    for (fext = fname+(strlen(fname)-1); fext >= fname; fext--)
    {
        if (*fext == '.') break; /* start of file extension found */
    }
    if (*fext == '.') fext++; /* point just past dot */
    else fext = "";

    if ((f = fopen(fname, "rb")) != NULL)
    {
        size_t sz = fread(buf.buffer, 1, BLOCKSIZE,f);
        if (!ferror(f))
        {
            fclose(f);  /* we've read in all we need so close */

            /* if size > sizeof(tar header), 1st compute header chksum
                and if matches chksum stored in header then assume tar (CM_NONE) */
            if ((sz >= sizeof(struct tar_header)) && valid_checksum(&(buf.header)))
                return CM_NONE;
            /* if file starts with "\037\0213" then assume gzipped (CM_GZ) */
            /* if file starts with "\037\0235" then assume compressed (CM_Z) [UNSUPPORTED] */
            if (buf.buffer[0] == '\x1F')
            {
                if (buf.buffer[1] == '\x8B') return CM_GZ;
                if (buf.buffer[1] == '\x9D') return CM_Z;
            }
            /* if file starts with "BZh" then assume bz2 (CM_BZ2) ("BZ"=bzip + version "h"=2) */
            if ((buf.buffer[0]=='B') && (buf.buffer[1]=='Z'))
                return CM_BZ2;
            /* if file extension .tgz or .gz then assume (CM_GZ) */
            if ((strcmpi(fext,"tgz")==0) || (strcmpi(fext,"gz")==0))
                return CM_GZ;
            /* if file extension .tbz or .bz2 then assume (CM_BZ2) */
            if ((strcmpi(fext,"tbz")==0) || (strcmpi(fext,"bz2")==0))
                return CM_BZ2;
            /* if file extension .lzma or .tlz then assume lzma (CM_LZMA) */
            /* otherwise if 1st byte valid lzma PROPERTY byte return CM_LZMA else CM_GZ */
            if ((strcmpi(fext,"tlz")==0) || (strcmpi(fext,"lzma")==0) || (buf.buffer[0] < (9*5*5)))
                return CM_LZMA;
        }
        else fclose(f);  /* cleanup */
    }
    /* if make it here then give up and return gzipped tarball */
    return CM_GZ;
}

