/* public domain, Kenneth J Davis, 2004 */

#include <stdlib.h>
#include <stdio.h>
#include <time.h> // for time() to pass to srand
#include <string.h> // for strcmpi()
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef unsigned long ULONG;
typedef unsigned char UCHAR;

UCHAR buffer[1024];  /* write up to a KB at a time */

/* fills buffer with semi random junk */
void fillInBuffer(void)
{
  UCHAR *p;
  UCHAR data = (UCHAR)rand();
  int i;
  for (i = sizeof(buffer)-1, p=buffer; i >= 0; i--, p++)
  {
    if (rand() % 2)  /* make semi-compressable file */
      data = (UCHAR)rand();
    *p = data;
  }
}


int main(int argc, char *argv[])
{
  unsigned randseed = (unsigned)time(NULL);
  ULONG maxfiles = 65536;
  ULONG minsize=1, maxsize=2048; /* file size in bytes, randomly between these */
  const char *basename = "TMP";  /* prepended to create path+filename */
  const char *suffix = ".$$$";   /* appended to created filename */
  char fname[260]; /* buffer used to create filename */
  for (int argno = 1; argno < argc; argno++)
  {
    if (strcmpi("/MAXFILES", argv[argno]) == 0)
    {
      argno++;
      maxfiles = strtoul(argv[argno], NULL, 0);
    }
    else if (strcmpi("/BASENAME", argv[argno]) == 0)
    {
      argno++;
      basename = argv[argno];
    }
    else if (strcmpi("/SUFFIX", argv[argno]) == 0)
    {
      argno++;
      suffix = argv[argno];
    }
    else if (strcmpi("/MAXSIZE", argv[argno]) == 0)
    {
      argno++;
      maxsize = strtoul(argv[argno], NULL, 0);
    }
    else if (strcmpi("/MINSIZE", argv[argno]) == 0)
    {
      argno++;
      minsize = strtoul(argv[argno], NULL, 0);
    }
    else if (strcmp("/RANDSEED", argv[argno]) == 0)
    {
      argno++;
      randseed = (unsigned)strtol(argv[argno], NULL, 0);
    }
  }
 
  printf("Creating %lu random files between %lu and %lu bytes with seed 0x%X\n",
         maxfiles, minsize, maxsize, randseed); 
  srand(randseed);

  for (ULONG fcnt = 0; fcnt < maxfiles; fcnt++)
  {
    HANDLE tempfile;
    sprintf(fname, "%s%lu%s", basename, fcnt, suffix);
    printf("%s\n", fname);
    tempfile = CreateFile(fname, GENERIC_WRITE, FILE_SHARE_READ, NULL, 
                          CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (tempfile == INVALID_HANDLE_VALUE)
    {
      printf("Aborting! failed to create %s\n", fname);
      exit(1);
    }
    
    ULONG count = 0;
    /* create up to minsize'd file */
    while (count < minsize)
    {
      DWORD bytesWritten;
      fillInBuffer();  /* fills in buffer with random data */
      unsigned cnt = (sizeof(buffer) > (minsize-count))?(unsigned)(minsize-count):sizeof(buffer);
      WriteFile(tempfile, buffer, cnt, &bytesWritten, 0);
      count += cnt;
      if (cnt != bytesWritten)
        printf("Warning: only wrote %u of %u bytes for %s\n", bytesWritten, cnt, fname);
    }
    while ((rand()%2) && (count < maxsize))
    {
      DWORD bytesWritten;
      fillInBuffer();  /* fills in buffer with random data */
      unsigned cnt = rand() % sizeof(buffer);
      if (cnt > (maxsize-count)) cnt = (unsigned)(maxsize-count);
      WriteFile(tempfile, buffer, cnt, &bytesWritten, 0);
      count += cnt;
      if (cnt != bytesWritten)
        printf("Warning: only wrote %u of %u bytes for %s\n", bytesWritten, cnt, fname);
    }
    CloseHandle(tempfile);
  }
  return 0;
}
