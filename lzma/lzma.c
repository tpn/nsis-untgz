#include "lzma.h"
#include "../miniclib.h"


/* !!!USER SUPPLIED!!! */
/* wrap around whatever you want to send error messages to user, c function */
void PrintMessage(const char *msg, ...);


/* holds any information needed about stream */

#define kInBufferSize (1 << 15)  /* 32KB */
typedef struct LZMAFile
{
  ILzmaInCallback InCallback;
  gzFile File;
  unsigned char Buffer[kInBufferSize];

  CLzmaDecoderState state;  /* it's about 24-80 bytes structure, if int is 32-bit */
  unsigned char properties[LZMA_PROPERTIES_SIZE];

  UInt32 outSize;     /* init to 0 */
  UInt32 outSizeHigh; /* init to 0 */
  int waitEOS;        /* init to 1 */
  /* waitEOS = 1, if there is no uncompressed size in headers, 
   so decoder will wait EOS (End of Stream Marker) in compressed stream */
} LZMAFile;

int LzmaReadCompressed(void *object, const unsigned char **buffer, SizeT *size)
{
  LZMAFile *b = (LZMAFile *)object;
  *buffer = b->Buffer;
  *size = (SizeT)gzread(b->File, b->Buffer, kInBufferSize);
  return LZMA_RESULT_OK;
}

/* returns nonzero if error reading */
int MyReadFileAndCheck(gzFile file, void *data, long size)
{ 
  register long len = gzread(file, data, size);
  if (len != size)
  {
    PrintMessage("lzma: Can not read input file.");
    return 0;
  }
  return 1;
} 


/* routines implemented */

/*
LZMA compressed file format
---------------------------
Offset Size Description
  0     1   Special LZMA properties for compressed data
  1     4   Dictionary size (little endian)
  5     8   Uncompressed size (little endian). -1 means unknown size
 13         Compressed data
*/

int lzma_init(gzFile infile, struct LZMAFile **lzmaFile)
{
  LZMAFile *inBuffer;
  *lzmaFile = inBuffer = (LZMAFile *)malloc(sizeof(struct LZMAFile));
  if (inBuffer == NULL) return -1;

  memset(inBuffer, 0, sizeof(LZMAFile));
  inBuffer->File = infile;
  inBuffer->InCallback.Read = LzmaReadCompressed;
  inBuffer->waitEOS = 1;


  /* Read LZMA properties for compressed stream */
  if (!MyReadFileAndCheck(infile, inBuffer->properties, LZMA_PROPERTIES_SIZE))
    return -1;

  /* Read uncompressed size */
  {
    int i;
    for (i = 0; i < 8; i++)
    {
      unsigned char b;
      if (!MyReadFileAndCheck(infile, &b, 1))
        return -1;
      if (b != 0xFF)
        inBuffer->waitEOS = 0;
      if (i < 4)
        inBuffer->outSize += (UInt32)(b) << (i * 8);
      else
        inBuffer->outSizeHigh += (UInt32)(b) << ((i - 4) * 8);
    }
  }

  /* Decode LZMA properties and allocate memory */
  if (LzmaDecodeProperties(&(inBuffer->state.Properties), inBuffer->properties, LZMA_PROPERTIES_SIZE) != LZMA_RESULT_OK)
  {
    PrintMessage("Incorrect stream properties");
    return -1;
  }
  inBuffer->state.Probs = (CProb *)malloc(LzmaGetNumProbs(&(inBuffer->state.Properties)) * sizeof(CProb));
  if (inBuffer->state.Probs == NULL) return -1;
  inBuffer->state.Dictionary = (unsigned char *)malloc(inBuffer->state.Properties.DictionarySize);
  if (inBuffer->state.Dictionary == NULL) return -1;

  LzmaDecoderInit(&(inBuffer->state));

  return 0;
}


void lzma_cleanup(struct LZMAFile *lzmaFile)
{
  if (lzmaFile != NULL)
  {  
    if (lzmaFile->state.Probs != NULL) free(lzmaFile->state.Probs);
    if (lzmaFile->state.Dictionary != NULL) free(lzmaFile->state.Dictionary);
    free(lzmaFile);
  }
}


long lzma_read(struct LZMAFile *lzmaFile,
               unsigned char *buffer,
               unsigned len)
{
  SizeT outProcessed;
  SizeT outAvail = len;
  int res;

  if (!lzmaFile->waitEOS && lzmaFile->outSizeHigh == 0 && outAvail > lzmaFile->outSize)
        outAvail = (SizeT)(lzmaFile->outSize);

  res = LzmaDecode(&(lzmaFile->state),
        &(lzmaFile->InCallback),
        buffer, outAvail, &outProcessed);
  if (res != 0)
  {
    PrintMessage("lzma_read: Decoding error (%d)", res);
    return -1;
  }
      
  if (lzmaFile->outSize < outProcessed)
    lzmaFile->outSizeHigh--;
  lzmaFile->outSize -= (UInt32)outProcessed;
  lzmaFile->outSize &= 0xFFFFFFFF;
        
  if (outProcessed == 0)
  {
    if (!lzmaFile->waitEOS && (lzmaFile->outSize != 0 || lzmaFile->outSizeHigh != 0))
    {
      PrintMessage("lzma_read: Unexpected EOS");
      return -1;
    }
  }

  return outProcessed;
}
