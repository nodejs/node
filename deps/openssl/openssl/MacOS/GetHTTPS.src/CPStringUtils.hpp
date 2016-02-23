#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void CopyPStrToCStr(const unsigned char *thePStr,char *theCStr,const int maxCStrLength);
void CopyPStrToPStr(const unsigned char *theSrcPStr,unsigned char *theDstPStr,const int maxDstStrLength);
void CopyCStrToCStr(const char *theSrcCStr,char *theDstCStr,const int maxDstStrLength);
void CopyCStrToPStr(const char *theSrcCStr,unsigned char *theDstPStr,const int maxDstStrLength);
void ConcatPStrToCStr(const unsigned char *thePStr,char *theCStr,const int maxCStrLength);
void ConcatPStrToPStr(const unsigned char *theSrcPStr,unsigned char *theDstPStr,const int maxDstStrLength);
void ConcatCStrToPStr(const char *theSrcCStr,unsigned char *theDstPStr,const int maxDstStrLength);
void ConcatCStrToCStr(const char *theSrcCStr,char *theDstCStr,const int maxCStrLength);

void ConcatCharToCStr(const char theChar,char *theDstCStr,const int maxCStrLength);
void ConcatCharToPStr(const char theChar,unsigned char *theDstPStr,const int maxPStrLength);

int ComparePStrs(const unsigned char *theFirstPStr,const unsigned char *theSecondPStr,const Boolean ignoreCase = true);
int CompareCStrs(const char *theFirstCStr,const char *theSecondCStr,const Boolean ignoreCase = true);
int CompareCStrToPStr(const char *theCStr,const unsigned char *thePStr,const Boolean ignoreCase = true);

Boolean CStrsAreEqual(const char *theFirstCStr,const char *theSecondCStr,const Boolean ignoreCase = true);
Boolean PStrsAreEqual(const unsigned char *theFirstCStr,const unsigned char *theSecondCStr,const Boolean ignoreCase = true);

void CopyLongIntToCStr(const long theNum,char *theCStr,const int maxCStrLength,const int numDigits = -1);
void CopyUnsignedLongIntToCStr(const unsigned long theNum,char *theCStr,const int maxCStrLength);
void ConcatLongIntToCStr(const long theNum,char *theCStr,const int maxCStrLength,const int numDigits = -1);
void CopyCStrAndConcatLongIntToCStr(const char *theSrcCStr,const long theNum,char *theDstCStr,const int maxDstStrLength);

void CopyLongIntToPStr(const long theNum,unsigned char *thePStr,const int maxPStrLength,const int numDigits = -1);
void ConcatLongIntToPStr(const long theNum,unsigned char *thePStr,const int maxPStrLength,const int numDigits = -1);

long CStrLength(const char *theCString);
long PStrLength(const unsigned char *thePString);

OSErr CopyCStrToExistingHandle(const char *theCString,Handle theHandle);
OSErr CopyLongIntToExistingHandle(const long inTheLongInt,Handle theHandle);

OSErr CopyCStrToNewHandle(const char *theCString,Handle *theHandle);
OSErr CopyPStrToNewHandle(const unsigned char *thePString,Handle *theHandle);
OSErr CopyLongIntToNewHandle(const long inTheLongInt,Handle *theHandle);

OSErr AppendCStrToHandle(const char *theCString,Handle theHandle,long *currentLength = nil,long *maxLength = nil);
OSErr AppendCharsToHandle(const char *theChars,const int numChars,Handle theHandle,long *currentLength = nil,long *maxLength = nil);
OSErr AppendPStrToHandle(const unsigned char *thePString,Handle theHandle,long *currentLength = nil);
OSErr AppendLongIntToHandle(const long inTheLongInt,Handle theHandle,long *currentLength = nil);

void ZeroMem(void *theMemPtr,const unsigned long numBytes);

char *FindCharInCStr(const char theChar,const char *theCString);
long FindCharOffsetInCStr(const char theChar,const char *theCString,const Boolean inIgnoreCase = false);
long FindCStrOffsetInCStr(const char *theCSubstring,const char *theCString,const Boolean inIgnoreCase = false);

void CopyCSubstrToCStr(const char *theSrcCStr,const int maxCharsToCopy,char *theDstCStr,const int maxDstStrLength);
void CopyCSubstrToPStr(const char *theSrcCStr,const int maxCharsToCopy,unsigned char *theDstPStr,const int maxDstStrLength);

void InsertCStrIntoCStr(const char *theSrcCStr,const int theInsertionOffset,char *theDstCStr,const int maxDstStrLength);
void InsertPStrIntoCStr(const unsigned char *theSrcPStr,const int theInsertionOffset,char *theDstCStr,const int maxDstStrLength);
OSErr InsertCStrIntoHandle(const char *theCString,Handle theHandle,const long inInsertOffset);

void CopyCStrAndInsertCStrIntoCStr(const char *theSrcCStr,const char *theInsertCStr,char *theDstCStr,const int maxDstStrLength);

void CopyCStrAndInsertCStrsLongIntsIntoCStr(const char *theSrcCStr,const char **theInsertCStrs,const long *theLongInts,char *theDstCStr,const int maxDstStrLength);

void CopyCStrAndInsert1LongIntIntoCStr(const char *theSrcCStr,const long theNum,char *theDstCStr,const int maxDstStrLength);
void CopyCStrAndInsert2LongIntsIntoCStr(const char *theSrcCStr,const long long1,const long long2,char *theDstCStr,const int maxDstStrLength);
void CopyCStrAndInsert3LongIntsIntoCStr(const char *theSrcCStr,const long long1,const long long2,const long long3,char *theDstCStr,const int maxDstStrLength);

void CopyCStrAndInsertCStrLongIntIntoCStr(const char *theSrcCStr,const char *theInsertCStr,const long theNum,char *theDstCStr,const int maxDstStrLength);
OSErr CopyCStrAndInsertCStrLongIntIntoHandle(const char *theSrcCStr,const char *theInsertCStr,const long theNum,Handle *theHandle);


OSErr CopyIndexedWordToCStr(char *theSrcCStr,int whichWord,char *theDstCStr,int maxDstCStrLength);
OSErr CopyIndexedWordToNewHandle(char *theSrcCStr,int whichWord,Handle *outTheHandle);

OSErr CopyIndexedLineToCStr(const char *theSrcCStr,int inWhichLine,int *lineEndIndex,Boolean *gotLastLine,char *theDstCStr,const int maxDstCStrLength);
OSErr CopyIndexedLineToNewHandle(const char *theSrcCStr,int inWhichLine,Handle *outNewHandle);

OSErr ExtractIntFromCStr(const char *theSrcCStr,int *outInt,Boolean skipLeadingSpaces = true);
OSErr ExtractIntFromPStr(const unsigned char *theSrcPStr,int *outInt,Boolean skipLeadingSpaces = true);


void ConvertCStrToUpperCase(char *theSrcCStr);


int CountOccurencesOfCharInCStr(const char inChar,const char *inSrcCStr);
int CountWordsInCStr(const char *inSrcCStr);

OSErr CountDigits(const char *inCStr,int *outNumIntegerDigits,int *outNumFractDigits);

void ExtractCStrItemFromCStr(const char *inSrcCStr,const char inItemDelimiter,const int inItemNumber,Boolean *foundItem,char *outDstCharPtr,const int inDstCharPtrMaxLength,const Boolean inTreatMultipleDelimsAsSingleDelim = false);
OSErr ExtractCStrItemFromCStrIntoNewHandle(const char *inSrcCStr,const char inItemDelimiter,const int inItemNumber,Boolean *foundItem,Handle *outNewHandle,const Boolean inTreatMultipleDelimsAsSingleDelim = false);


OSErr ExtractFloatFromCStr(const char *inCString,extended80 *outFloat);
OSErr CopyFloatToCStr(const extended80 *theFloat,char *theCStr,const int maxCStrLength,const int inMaxNumIntDigits = -1,const int inMaxNumFractDigits = -1);

void SkipWhiteSpace(char **ioSrcCharPtr,const Boolean inStopAtEOL = false);


#ifdef __cplusplus
}
#endif
