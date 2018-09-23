/* ====================================================================
 * Copyright (c) 1998-1999 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */
 
 
 
 #include "CPStringUtils.hpp"
#include "ErrorHandling.hpp"



#define kNumberFormatString			"\p########0.00#######;-########0.00#######"



//	Useful utility functions which could be optimized a whole lot


void CopyPStrToCStr(const unsigned char *thePStr,char *theCStr,const int maxCStrLength)
{
int		i,numPChars;


	if (thePStr != nil && theCStr != nil && maxCStrLength > 0)
	{
		numPChars = thePStr[0];
		
		for (i = 0;;i++)
		{
			if (i >= numPChars || i >= maxCStrLength - 1)
			{
				theCStr[i] = 0;
				
				break;
			}
			
			else
			{
				theCStr[i] = thePStr[i + 1];
			}
		}
	}
}


void CopyPStrToPStr(const unsigned char *theSrcPStr,unsigned char *theDstPStr,const int maxDstStrLength)
{
int		theMaxDstStrLength;

	
	theMaxDstStrLength = maxDstStrLength;
	
	
	if (theDstPStr != nil && theSrcPStr != nil && theMaxDstStrLength > 0)
	{
		if (theMaxDstStrLength > 255)
		{
			theMaxDstStrLength = 255;
		}
		
		
		if (theMaxDstStrLength - 1 < theSrcPStr[0])
		{
			BlockMove(theSrcPStr + 1,theDstPStr + 1,theMaxDstStrLength - 1);
			
			theDstPStr[0] = theMaxDstStrLength - 1;
		}
		
		else
		{
			BlockMove(theSrcPStr,theDstPStr,theSrcPStr[0] + 1);
		}
	}
}


void CopyCStrToCStr(const char *theSrcCStr,char *theDstCStr,const int maxDstStrLength)
{
int		i;


	if (theDstCStr != nil && theSrcCStr != nil && maxDstStrLength > 0)
	{
		for (i = 0;;i++)
		{
			if (theSrcCStr[i] == 0 || i >= maxDstStrLength - 1)
			{
				theDstCStr[i] = 0;
				
				break;
			}
			
			else
			{
				theDstCStr[i] = theSrcCStr[i];
			}
		}
	}
}



void CopyCSubstrToCStr(const char *theSrcCStr,const int maxCharsToCopy,char *theDstCStr,const int maxDstStrLength)
{
int		i;


	if (theDstCStr != nil && theSrcCStr != nil && maxDstStrLength > 0)
	{
		for (i = 0;;i++)
		{
			if (theSrcCStr[i] == 0 || i >= maxDstStrLength - 1 || i >= maxCharsToCopy)
			{
				theDstCStr[i] = 0;
				
				break;
			}
			
			else
			{
				theDstCStr[i] = theSrcCStr[i];
			}
		}
	}
}



void CopyCSubstrToPStr(const char *theSrcCStr,const int maxCharsToCopy,unsigned char *theDstPStr,const int maxDstStrLength)
{
int		i;
int		theMaxDstStrLength;

	
	theMaxDstStrLength = maxDstStrLength;

	if (theDstPStr != nil && theSrcCStr != nil && theMaxDstStrLength > 0)
	{
		if (theMaxDstStrLength > 255)
		{
			theMaxDstStrLength = 255;
		}
		
		
		for (i = 0;;i++)
		{
			if (theSrcCStr[i] == 0 || i >= theMaxDstStrLength - 1 || i >= maxCharsToCopy)
			{
				theDstPStr[0] = i;
				
				break;
			}
			
			else
			{
				theDstPStr[i + 1] = theSrcCStr[i];
			}
		}
	}
}



void CopyCStrToPStr(const char *theSrcCStr,unsigned char *theDstPStr,const int maxDstStrLength)
{
int		i;
int		theMaxDstStrLength;

	
	theMaxDstStrLength = maxDstStrLength;

	if (theDstPStr != nil && theSrcCStr != nil && theMaxDstStrLength > 0)
	{
		if (theMaxDstStrLength > 255)
		{
			theMaxDstStrLength = 255;
		}
		
		
		for (i = 0;;i++)
		{
			if (i >= theMaxDstStrLength - 1 || theSrcCStr[i] == 0)
			{
				theDstPStr[0] = i;
				
				break;
			}
			
			else
			{
				theDstPStr[i + 1] = theSrcCStr[i];
			}
		}
	}
}


void ConcatPStrToCStr(const unsigned char *thePStr,char *theCStr,const int maxCStrLength)
{
int		i,numPChars,cStrLength;


	if (thePStr != nil && theCStr != nil && maxCStrLength > 0)
	{
		for (cStrLength = 0;theCStr[cStrLength] != 0;cStrLength++)
		{
		
		}
		

		numPChars = thePStr[0];
		
		
		for (i = 0;;i++)
		{
			if (i >= numPChars || cStrLength >= maxCStrLength - 1)
			{
				theCStr[cStrLength++] = 0;
				
				break;
			}
			
			else
			{
				theCStr[cStrLength++] = thePStr[i + 1];
			}
		}
	}
}



void ConcatPStrToPStr(const unsigned char *theSrcPStr,unsigned char *theDstPStr,const int maxDstStrLength)
{
int		theMaxDstStrLength;

	
	theMaxDstStrLength = maxDstStrLength;
	
	if (theSrcPStr != nil && theDstPStr != nil && theMaxDstStrLength > 0)
	{
		if (theMaxDstStrLength > 255)
		{
			theMaxDstStrLength = 255;
		}
		
		
		if (theMaxDstStrLength - theDstPStr[0] - 1 < theSrcPStr[0])
		{
			BlockMove(theSrcPStr + 1,theDstPStr + theDstPStr[0] + 1,theMaxDstStrLength - 1 - theDstPStr[0]);
			
			theDstPStr[0] = theMaxDstStrLength - 1;
		}
		
		else
		{
			BlockMove(theSrcPStr + 1,theDstPStr + theDstPStr[0] + 1,theSrcPStr[0]);
			
			theDstPStr[0] += theSrcPStr[0];
		}
	}
}



void ConcatCStrToPStr(const char *theSrcCStr,unsigned char *theDstPStr,const int maxDstStrLength)
{
int		i,thePStrLength;
int		theMaxDstStrLength;

	
	theMaxDstStrLength = maxDstStrLength;

	if (theSrcCStr != nil && theDstPStr != nil && theMaxDstStrLength > 0)
	{
		if (theMaxDstStrLength > 255)
		{
			theMaxDstStrLength = 255;
		}
		
		
		thePStrLength = theDstPStr[0];
		
		for (i = 0;;i++)
		{
			if (theSrcCStr[i] == 0 || thePStrLength >= theMaxDstStrLength - 1)
			{
				theDstPStr[0] = thePStrLength;
				
				break;
			}
			
			else
			{
				theDstPStr[thePStrLength + 1] = theSrcCStr[i];
				
				thePStrLength++;
			}
		}
	}
}



void ConcatCStrToCStr(const char *theSrcCStr,char *theDstCStr,const int maxCStrLength)
{
int		cStrLength;


	if (theSrcCStr != nil && theDstCStr != nil && maxCStrLength > 0)
	{
		for (cStrLength = 0;theDstCStr[cStrLength] != 0;cStrLength++)
		{
		
		}
		

		for (;;)
		{
			if (*theSrcCStr == 0 || cStrLength >= maxCStrLength - 1)
			{
				theDstCStr[cStrLength++] = 0;
				
				break;
			}
			
			else
			{
				theDstCStr[cStrLength++] = *theSrcCStr++;
			}
		}
	}
}



void ConcatCharToCStr(const char theChar,char *theDstCStr,const int maxCStrLength)
{
int		cStrLength;


	if (theDstCStr != nil && maxCStrLength > 0)
	{
		cStrLength = CStrLength(theDstCStr);
		
		if (cStrLength < maxCStrLength - 1)
		{
			theDstCStr[cStrLength++] = theChar;
			theDstCStr[cStrLength++] = '\0';
		}
	}
}



void ConcatCharToPStr(const char theChar,unsigned char *theDstPStr,const int maxPStrLength)
{
int		pStrLength;


	if (theDstPStr != nil && maxPStrLength > 0)
	{
		pStrLength = PStrLength(theDstPStr);
		
		if (pStrLength < maxPStrLength - 1 && pStrLength < 255)
		{
			theDstPStr[pStrLength + 1] = theChar;
			theDstPStr[0] += 1;
		}
	}
}




int CompareCStrs(const char *theFirstCStr,const char *theSecondCStr,const Boolean ignoreCase)
{
int		returnValue;
char	firstChar,secondChar;

	
	returnValue = 0;
	
	
	if (theFirstCStr != nil && theSecondCStr != nil)
	{
		for (;;)
		{
			firstChar = *theFirstCStr;
			secondChar = *theSecondCStr;
			
			if (ignoreCase == true)
			{
				if (firstChar >= 'A' && firstChar <= 'Z')
				{
					firstChar = 'a' + (firstChar - 'A');
				}
				
				if (secondChar >= 'A' && secondChar <= 'Z')
				{
					secondChar = 'a' + (secondChar - 'A');
				}
			}
			
			
			if (firstChar == 0 && secondChar != 0)
			{
				returnValue = -1;
				
				break;
			}
			
			else if (firstChar != 0 && secondChar == 0)
			{
				returnValue = 1;
				
				break;
			}
			
			else if (firstChar == 0 && secondChar == 0)
			{
				returnValue = 0;
				
				break;
			}
			
			else if (firstChar < secondChar)
			{
				returnValue = -1;
				
				break;
			}
			
			else if (firstChar > secondChar)
			{
				returnValue = 1;
				
				break;
			}
			
			theFirstCStr++;
			theSecondCStr++;
		}
	}
	
	
	return(returnValue);
}



Boolean CStrsAreEqual(const char *theFirstCStr,const char *theSecondCStr,const Boolean ignoreCase)
{
	if (CompareCStrs(theFirstCStr,theSecondCStr,ignoreCase) == 0)
	{
		return true;
	}
	
	else
	{
		return false;
	}
}


Boolean PStrsAreEqual(const unsigned char *theFirstPStr,const unsigned char *theSecondPStr,const Boolean ignoreCase)
{
	if (ComparePStrs(theFirstPStr,theSecondPStr,ignoreCase) == 0)
	{
		return true;
	}
	
	else
	{
		return false;
	}
}



int ComparePStrs(const unsigned char *theFirstPStr,const unsigned char *theSecondPStr,const Boolean ignoreCase)
{
int		i,returnValue;
char	firstChar,secondChar;

	
	returnValue = 0;
	
	
	if (theFirstPStr != nil && theSecondPStr != nil)
	{
		for (i = 1;;i++)
		{
			firstChar = theFirstPStr[i];
			secondChar = theSecondPStr[i];

			if (ignoreCase == true)
			{
				if (firstChar >= 'A' && firstChar <= 'Z')
				{
					firstChar = 'a' + (firstChar - 'A');
				}
				
				if (secondChar >= 'A' && secondChar <= 'Z')
				{
					secondChar = 'a' + (secondChar - 'A');
				}
			}


			if (theFirstPStr[0] < i && theSecondPStr[0] >= i)
			{
				returnValue = -1;
				
				break;
			}
			
			else if (theFirstPStr[0] >= i && theSecondPStr[0] < i)
			{
				returnValue = 1;
				
				break;
			}
			
			else if (theFirstPStr[0] < i && theSecondPStr[0] < i)
			{
				returnValue = 0;
				
				break;
			}
			
			else if (firstChar < secondChar)
			{
				returnValue = -1;
				
				break;
			}
			
			else if (firstChar > secondChar)
			{
				returnValue = 1;
				
				break;
			}
		}
	}
	
	
	return(returnValue);
}



int CompareCStrToPStr(const char *theCStr,const unsigned char *thePStr,const Boolean ignoreCase)
{
int		returnValue;
char	tempString[256];

	
	returnValue = 0;
	
	if (theCStr != nil && thePStr != nil)
	{
		CopyPStrToCStr(thePStr,tempString,sizeof(tempString));
		
		returnValue = CompareCStrs(theCStr,tempString,ignoreCase);
	}
	
	
	return(returnValue);
}



void ConcatLongIntToCStr(const long theNum,char *theCStr,const int maxCStrLength,const int numDigits)
{
Str255 		theStr255;


	NumToString(theNum,theStr255);


	if (numDigits > 0)
	{
	int 	charsToInsert;
	
		
		charsToInsert = numDigits - PStrLength(theStr255);
		
		if (charsToInsert > 0)
		{
		char	tempString[256];
			
			CopyCStrToCStr("",tempString,sizeof(tempString));
			
			for (;charsToInsert > 0;charsToInsert--)
			{
				ConcatCStrToCStr("0",tempString,sizeof(tempString));
			}
			
			ConcatPStrToCStr(theStr255,tempString,sizeof(tempString));
			
			CopyCStrToPStr(tempString,theStr255,sizeof(theStr255));
		}
	}


	ConcatPStrToCStr(theStr255,theCStr,maxCStrLength);
}




void ConcatLongIntToPStr(const long theNum,unsigned char *thePStr,const int maxPStrLength,const int numDigits)
{
Str255 		theStr255;


	NumToString(theNum,theStr255);


	if (numDigits > 0)
	{
	int 	charsToInsert;
	
		
		charsToInsert = numDigits - PStrLength(theStr255);
		
		if (charsToInsert > 0)
		{
		char	tempString[256];
			
			CopyCStrToCStr("",tempString,sizeof(tempString));
			
			for (;charsToInsert > 0;charsToInsert--)
			{
				ConcatCStrToCStr("0",tempString,sizeof(tempString));
			}
			
			ConcatPStrToCStr(theStr255,tempString,sizeof(tempString));
			
			CopyCStrToPStr(tempString,theStr255,sizeof(theStr255));
		}
	}


	ConcatPStrToPStr(theStr255,thePStr,maxPStrLength);
}



void CopyCStrAndConcatLongIntToCStr(const char *theSrcCStr,const long theNum,char *theDstCStr,const int maxDstStrLength)
{
	CopyCStrToCStr(theSrcCStr,theDstCStr,maxDstStrLength);
	
	ConcatLongIntToCStr(theNum,theDstCStr,maxDstStrLength);
}



void CopyLongIntToCStr(const long theNum,char *theCStr,const int maxCStrLength,const int numDigits)
{
Str255 		theStr255;


	NumToString(theNum,theStr255);


	if (numDigits > 0)
	{
	int 	charsToInsert;
	
		
		charsToInsert = numDigits - PStrLength(theStr255);
		
		if (charsToInsert > 0)
		{
		char	tempString[256];
			
			CopyCStrToCStr("",tempString,sizeof(tempString));
			
			for (;charsToInsert > 0;charsToInsert--)
			{
				ConcatCStrToCStr("0",tempString,sizeof(tempString));
			}
			
			ConcatPStrToCStr(theStr255,tempString,sizeof(tempString));
			
			CopyCStrToPStr(tempString,theStr255,sizeof(theStr255));
		}
	}


	CopyPStrToCStr(theStr255,theCStr,maxCStrLength);
}





void CopyUnsignedLongIntToCStr(const unsigned long theNum,char *theCStr,const int maxCStrLength)
{
char			tempString[256];
int				srcCharIndex,dstCharIndex;
unsigned long	tempNum,quotient,remainder;

	
	if (theNum == 0)
	{
		CopyCStrToCStr("0",theCStr,maxCStrLength);
	}
	
	else
	{
		srcCharIndex = 0;
		
		tempNum = theNum;
		
		for (;;)
		{
			if (srcCharIndex >= sizeof(tempString) - 1 || tempNum == 0)
			{
				for (dstCharIndex = 0;;)
				{
					if (dstCharIndex >= maxCStrLength - 1 || srcCharIndex <= 0)
					{
						theCStr[dstCharIndex] = 0;
						
						break;
					}
					
					theCStr[dstCharIndex++] = tempString[--srcCharIndex];
				}
				
				break;
			}
			

			quotient = tempNum / 10;
			
			remainder = tempNum - (quotient * 10);
			
			tempString[srcCharIndex] = '0' + remainder;
			
			srcCharIndex++;
			
			tempNum = quotient;
		}
	}
}




void CopyLongIntToPStr(const long theNum,unsigned char *thePStr,const int maxPStrLength,const int numDigits)
{
char	tempString[256];


	CopyLongIntToCStr(theNum,tempString,sizeof(tempString),numDigits);
	
	CopyCStrToPStr(tempString,thePStr,maxPStrLength);
}



OSErr CopyLongIntToNewHandle(const long inTheLongInt,Handle *theHandle)
{
OSErr		errCode = noErr;
char		tempString[32];
	
	
	CopyLongIntToCStr(inTheLongInt,tempString,sizeof(tempString));
	
	errCode = CopyCStrToNewHandle(tempString,theHandle);

	return(errCode);
}


OSErr CopyLongIntToExistingHandle(const long inTheLongInt,Handle theHandle)
{
OSErr		errCode = noErr;
char		tempString[32];
	
	
	CopyLongIntToCStr(inTheLongInt,tempString,sizeof(tempString));
	
	errCode = CopyCStrToExistingHandle(tempString,theHandle);

	return(errCode);
}




OSErr CopyCStrToExistingHandle(const char *theCString,Handle theHandle)
{
OSErr	errCode = noErr;
long	stringLength;

	
	if (theCString == nil)
	{
		SetErrorMessageAndBail(("CopyCStrToExistingHandle: Bad parameter, theCString == nil"));
	}

	if (theHandle == nil)
	{
		SetErrorMessageAndBail(("CopyCStrToExistingHandle: Bad parameter, theHandle == nil"));
	}

	if (*theHandle == nil)
	{
		SetErrorMessageAndBail(("CopyCStrToExistingHandle: Bad parameter, *theHandle == nil"));
	}



	stringLength = CStrLength(theCString) + 1;
	
	SetHandleSize(theHandle,stringLength);
	
	if (GetHandleSize(theHandle) < stringLength)
	{
		SetErrorMessageAndLongIntAndBail("CopyCStrToExistingHandle: Can't set Handle size, MemError() = ",MemError());
	}
	
	
	::BlockMove(theCString,*theHandle,stringLength);
	

EXITPOINT:
	
	return(errCode);
}





OSErr CopyCStrToNewHandle(const char *theCString,Handle *theHandle)
{
OSErr	errCode = noErr;
long	stringLength;

	
	if (theCString == nil)
	{
		SetErrorMessageAndBail(("CopyCStrToNewHandle: Bad parameter, theCString == nil"));
	}

	if (theHandle == nil)
	{
		SetErrorMessageAndBail(("CopyCStrToNewHandle: Bad parameter, theHandle == nil"));
	}



	stringLength = CStrLength(theCString) + 1;
	
	*theHandle = NewHandle(stringLength);
	
	if (*theHandle == nil)
	{
		SetErrorMessageAndLongIntAndBail("CopyCStrToNewHandle: Can't allocate Handle, MemError() = ",MemError());
	}
	
	
	::BlockMove(theCString,**theHandle,stringLength);
	

EXITPOINT:
	
	return(errCode);
}



OSErr CopyPStrToNewHandle(const unsigned char *thePString,Handle *theHandle)
{
OSErr	errCode = noErr;
long	stringLength;

	
	if (thePString == nil)
	{
		SetErrorMessageAndBail(("CopyPStrToNewHandle: Bad parameter, thePString == nil"));
	}

	if (theHandle == nil)
	{
		SetErrorMessageAndBail(("CopyPStrToNewHandle: Bad parameter, theHandle == nil"));
	}



	stringLength = PStrLength(thePString) + 1;
	
	*theHandle = NewHandle(stringLength);
	
	if (*theHandle == nil)
	{
		SetErrorMessageAndLongIntAndBail("CopyPStrToNewHandle: Can't allocate Handle, MemError() = ",MemError());
	}
	
	
	if (stringLength > 1)
	{
		BlockMove(thePString + 1,**theHandle,stringLength - 1);
	}
	
	(**theHandle)[stringLength - 1] = 0;
	

EXITPOINT:
	
	return(errCode);
}


OSErr AppendPStrToHandle(const unsigned char *thePString,Handle theHandle,long *currentLength)
{
OSErr		errCode = noErr;
char		tempString[256];

	
	CopyPStrToCStr(thePString,tempString,sizeof(tempString));
	
	errCode = AppendCStrToHandle(tempString,theHandle,currentLength);
	

EXITPOINT:
	
	return(errCode);
}



OSErr AppendCStrToHandle(const char *theCString,Handle theHandle,long *currentLength,long *maxLength)
{
OSErr		errCode = noErr;
long		handleMaxLength,handleCurrentLength,stringLength,byteCount;


	if (theCString == nil)
	{
		SetErrorMessageAndBail(("AppendCStrToHandle: Bad parameter, theCString == nil"));
	}

	if (theHandle == nil)
	{
		SetErrorMessageAndBail(("AppendCStrToHandle: Bad parameter, theHandle == nil"));
	}
	
	
	if (maxLength != nil)
	{
		handleMaxLength = *maxLength;
	}
	
	else
	{
		handleMaxLength = GetHandleSize(theHandle);
	}
	
	
	if (currentLength != nil && *currentLength >= 0)
	{
		handleCurrentLength = *currentLength;
	}
	
	else
	{
		handleCurrentLength = CStrLength(*theHandle);
	}
	
	
	stringLength = CStrLength(theCString);
	
	byteCount = handleCurrentLength + stringLength + 1;
	
	if (byteCount > handleMaxLength)
	{
		SetHandleSize(theHandle,handleCurrentLength + stringLength + 1);
		
		if (maxLength != nil)
		{
			*maxLength = GetHandleSize(theHandle);
			
			handleMaxLength = *maxLength;
		}
		
		else
		{
			handleMaxLength = GetHandleSize(theHandle);
		}

		if (byteCount > handleMaxLength)
		{
			SetErrorMessageAndLongIntAndBail("AppendCStrToHandle: Can't increase Handle allocation, MemError() = ",MemError());
		}
	}
	
	
	BlockMove(theCString,*theHandle + handleCurrentLength,stringLength + 1);
	
	
	if (currentLength != nil)
	{
		*currentLength += stringLength;
	}


	errCode = noErr;
	
	
EXITPOINT:

	return(errCode);
}



OSErr AppendCharsToHandle(const char *theChars,const int numChars,Handle theHandle,long *currentLength,long *maxLength)
{
OSErr		errCode = noErr;
long		handleMaxLength,handleCurrentLength,byteCount;


	if (theChars == nil)
	{
		SetErrorMessageAndBail(("AppendCharsToHandle: Bad parameter, theChars == nil"));
	}

	if (theHandle == nil)
	{
		SetErrorMessageAndBail(("AppendCharsToHandle: Bad parameter, theHandle == nil"));
	}
	
	
	if (maxLength != nil)
	{
		handleMaxLength = *maxLength;
	}
	
	else
	{
		handleMaxLength = GetHandleSize(theHandle);
	}
	
	
	if (currentLength != nil && *currentLength >= 0)
	{
		handleCurrentLength = *currentLength;
	}
	
	else
	{
		handleCurrentLength = CStrLength(*theHandle);
	}
	
	
	byteCount = handleCurrentLength + numChars + 1;
	
	if (byteCount > handleMaxLength)
	{
		SetHandleSize(theHandle,handleCurrentLength + numChars + 1);
		
		if (maxLength != nil)
		{
			*maxLength = GetHandleSize(theHandle);
			
			handleMaxLength = *maxLength;
		}
		
		else
		{
			handleMaxLength = GetHandleSize(theHandle);
		}

		if (byteCount > handleMaxLength)
		{
			SetErrorMessageAndLongIntAndBail("AppendCharsToHandle: Can't increase Handle allocation, MemError() = ",MemError());
		}
	}
	
	
	BlockMove(theChars,*theHandle + handleCurrentLength,numChars);
	
	(*theHandle)[handleCurrentLength + numChars] = '\0';
	
	if (currentLength != nil)
	{
		*currentLength += numChars;
	}


	errCode = noErr;
	
	
EXITPOINT:

	return(errCode);
}



OSErr AppendLongIntToHandle(const long inTheLongInt,Handle theHandle,long *currentLength)
{
OSErr		errCode = noErr;
char		tempString[32];
	
	
	CopyLongIntToCStr(inTheLongInt,tempString,sizeof(tempString));
	
	errCode = AppendCStrToHandle(tempString,theHandle,currentLength);

	return(errCode);
}




long CStrLength(const char *theCString)
{
long	cStrLength = 0;

	
	if (theCString != nil)
	{
		for (cStrLength = 0;theCString[cStrLength] != 0;cStrLength++)
		{
		
		}
	}
	
	
	return(cStrLength);
}



long PStrLength(const unsigned char *thePString)
{
long	pStrLength = 0;

	
	if (thePString != nil)
	{
		pStrLength = thePString[0];
	}
	
	
	return(pStrLength);
}





void ZeroMem(void *theMemPtr,const unsigned long numBytes)
{
unsigned char	*theBytePtr;
unsigned long	*theLongPtr;
unsigned long	numSingleBytes;
unsigned long	theNumBytes;

	
	theNumBytes = numBytes;
	
	if (theMemPtr != nil && theNumBytes > 0)
	{
		theBytePtr = (unsigned char	*) theMemPtr;
		
		numSingleBytes = (unsigned long) theBytePtr & 0x0003;
		
		while (numSingleBytes > 0)
		{
			*theBytePtr++ = 0;
			
			theNumBytes--;
			numSingleBytes--;
		}
		

		theLongPtr = (unsigned long	*) theBytePtr;
		
		while (theNumBytes >= 4)
		{
			*theLongPtr++ = 0;
			
			theNumBytes -= 4;
		}
		
		
		theBytePtr = (unsigned char	*) theLongPtr;
		
		while (theNumBytes > 0)
		{
			*theBytePtr++ = 0;
			
			theNumBytes--;
		}
	}
}




char *FindCharInCStr(const char theChar,const char *theCString)
{
char	*theStringSearchPtr;

	
	theStringSearchPtr = (char	*) theCString;
	
	if (theStringSearchPtr != nil)
	{
		while (*theStringSearchPtr != '\0' && *theStringSearchPtr != theChar)
		{
			theStringSearchPtr++;
		}
		
		if (*theStringSearchPtr == '\0')
		{
			theStringSearchPtr = nil;
		}
	}
	
	return(theStringSearchPtr);
}



long FindCharOffsetInCStr(const char theChar,const char *theCString,const Boolean inIgnoreCase)
{
long	theOffset = -1;


	if (theCString != nil)
	{
		theOffset = 0;
		

		if (inIgnoreCase)
		{
		char	searchChar = theChar;
		
			if (searchChar >= 'a' && searchChar <= 'z')
			{
				searchChar = searchChar - 'a' + 'A';
			}
			
			
			while (*theCString != 0)
			{
			char	currentChar = *theCString;
			
				if (currentChar >= 'a' && currentChar <= 'z')
				{
					currentChar = currentChar - 'a' + 'A';
				}
			
				if (currentChar == searchChar)
				{
					break;
				}
				
				theCString++;
				theOffset++;
			}
		}
		
		else
		{
			while (*theCString != 0 && *theCString != theChar)
			{
				theCString++;
				theOffset++;
			}
		}
		
		if (*theCString == 0)
		{
			theOffset = -1;
		}
	}
	
	return(theOffset);
}


long FindCStrOffsetInCStr(const char *theCSubstring,const char *theCString,const Boolean inIgnoreCase)
{
long	theOffset = -1;


	if (theCSubstring != nil && theCString != nil)
	{
		for (theOffset = 0;;theOffset++)
		{
			if (theCString[theOffset] == 0)
			{
				theOffset = -1;
				
				goto EXITPOINT;
			}
			
			
			for (const char	*tempSubstringPtr = theCSubstring,*tempCStringPtr = theCString + theOffset;;tempSubstringPtr++,tempCStringPtr++)
			{
				if (*tempSubstringPtr == 0)
				{
					goto EXITPOINT;
				}
				
				else if (*tempCStringPtr == 0)
				{
					break;
				}
			
			char	searchChar = *tempSubstringPtr;
			char	currentChar = *tempCStringPtr;
			
				if (inIgnoreCase && searchChar >= 'a' && searchChar <= 'z')
				{
					searchChar = searchChar - 'a' + 'A';
				}
				
				if (inIgnoreCase && currentChar >= 'a' && currentChar <= 'z')
				{
					currentChar = currentChar - 'a' + 'A';
				}
				
				if (currentChar != searchChar)
				{
					break;
				}
			}
		}
		
		theOffset = -1;
	}


EXITPOINT:
	
	return(theOffset);
}



void InsertCStrIntoCStr(const char *theSrcCStr,const int theInsertionOffset,char *theDstCStr,const int maxDstStrLength)
{
int		currentLength;
int		insertLength;
int		numCharsToInsert;
int		numCharsToShift;

	
	if (theDstCStr != nil && theSrcCStr != nil && maxDstStrLength > 0 && theInsertionOffset < maxDstStrLength - 1)
	{
		currentLength = CStrLength(theDstCStr);
		
		insertLength = CStrLength(theSrcCStr);
		

		if (theInsertionOffset + insertLength < maxDstStrLength - 1)
		{
			numCharsToInsert = insertLength;
		}
		
		else
		{
			numCharsToInsert = maxDstStrLength - 1 - theInsertionOffset;
		}
		

		if (numCharsToInsert + currentLength < maxDstStrLength - 1)
		{
			numCharsToShift = currentLength - theInsertionOffset;
		}
		
		else
		{
			numCharsToShift = maxDstStrLength - 1 - theInsertionOffset - numCharsToInsert;
		}

		
		if (numCharsToShift > 0)
		{
			BlockMove(theDstCStr + theInsertionOffset,theDstCStr + theInsertionOffset + numCharsToInsert,numCharsToShift);
		}
		
		if (numCharsToInsert > 0)
		{
			BlockMove(theSrcCStr,theDstCStr + theInsertionOffset,numCharsToInsert);
		}
		
		theDstCStr[theInsertionOffset + numCharsToInsert + numCharsToShift] = 0;
	}
}



void InsertPStrIntoCStr(const unsigned char *theSrcPStr,const int theInsertionOffset,char *theDstCStr,const int maxDstStrLength)
{
int		currentLength;
int		insertLength;
int		numCharsToInsert;
int		numCharsToShift;

	
	if (theDstCStr != nil && theSrcPStr != nil && maxDstStrLength > 0 && theInsertionOffset < maxDstStrLength - 1)
	{
		currentLength = CStrLength(theDstCStr);
		
		insertLength = PStrLength(theSrcPStr);
		

		if (theInsertionOffset + insertLength < maxDstStrLength - 1)
		{
			numCharsToInsert = insertLength;
		}
		
		else
		{
			numCharsToInsert = maxDstStrLength - 1 - theInsertionOffset;
		}
		

		if (numCharsToInsert + currentLength < maxDstStrLength - 1)
		{
			numCharsToShift = currentLength - theInsertionOffset;
		}
		
		else
		{
			numCharsToShift = maxDstStrLength - 1 - theInsertionOffset - numCharsToInsert;
		}

		
		if (numCharsToShift > 0)
		{
			BlockMove(theDstCStr + theInsertionOffset,theDstCStr + theInsertionOffset + numCharsToInsert,numCharsToShift);
		}
		
		if (numCharsToInsert > 0)
		{
			BlockMove(theSrcPStr + 1,theDstCStr + theInsertionOffset,numCharsToInsert);
		}
		
		theDstCStr[theInsertionOffset + numCharsToInsert + numCharsToShift] = 0;
	}
}



OSErr InsertCStrIntoHandle(const char *theCString,Handle theHandle,const long inInsertOffset)
{
OSErr	errCode;
int		currentLength;
int		insertLength;

	
	SetErrorMessageAndBailIfNil(theCString,"InsertCStrIntoHandle: Bad parameter, theCString == nil");

	SetErrorMessageAndBailIfNil(theHandle,"InsertCStrIntoHandle: Bad parameter, theHandle == nil");
	
	currentLength = CStrLength(*theHandle);
	
	if (currentLength + 1 > ::GetHandleSize(theHandle))
	{
		SetErrorMessageAndBail("InsertCStrIntoHandle: Handle has been overflowed");
	}
	
	if (inInsertOffset > currentLength)
	{
		SetErrorMessageAndBail("InsertCStrIntoHandle: Insertion offset is greater than string length");
	}
	
	insertLength = CStrLength(theCString);
	
	::SetHandleSize(theHandle,currentLength + 1 + insertLength);
	
	if (::GetHandleSize(theHandle) < currentLength + 1 + insertLength)
	{
		SetErrorMessageAndLongIntAndBail("InsertCStrIntoHandle: Can't expand storage for Handle, MemError() = ",MemError());
	}
	
	::BlockMove(*theHandle + inInsertOffset,*theHandle + inInsertOffset + insertLength,currentLength - inInsertOffset + 1);
	
	::BlockMove(theCString,*theHandle + inInsertOffset,insertLength);


	errCode = noErr;
	
	
EXITPOINT:

	return(errCode);
}




void CopyCStrAndInsert1LongIntIntoCStr(const char *theSrcCStr,const long theNum,char *theDstCStr,const int maxDstStrLength)
{
	CopyCStrAndInsertCStrLongIntIntoCStr(theSrcCStr,nil,theNum,theDstCStr,maxDstStrLength);
}


void CopyCStrAndInsert2LongIntsIntoCStr(const char *theSrcCStr,const long long1,const long long2,char *theDstCStr,const int maxDstStrLength)
{
const long	theLongInts[] = { long1,long2 };

	CopyCStrAndInsertCStrsLongIntsIntoCStr(theSrcCStr,nil,theLongInts,theDstCStr,maxDstStrLength);
}


void CopyCStrAndInsert3LongIntsIntoCStr(const char *theSrcCStr,const long long1,const long long2,const long long3,char *theDstCStr,const int maxDstStrLength)
{
const long	theLongInts[] = { long1,long2,long3 };

	CopyCStrAndInsertCStrsLongIntsIntoCStr(theSrcCStr,nil,theLongInts,theDstCStr,maxDstStrLength);
}


void CopyCStrAndInsertCStrIntoCStr(const char *theSrcCStr,const char *theInsertCStr,char *theDstCStr,const int maxDstStrLength)
{
const char	*theCStrs[2] = { theInsertCStr,nil };

	CopyCStrAndInsertCStrsLongIntsIntoCStr(theSrcCStr,theCStrs,nil,theDstCStr,maxDstStrLength);
}



void CopyCStrAndInsertCStrLongIntIntoCStr(const char *theSrcCStr,const char *theInsertCStr,const long theNum,char *theDstCStr,const int maxDstStrLength)
{
const char	*theCStrs[2] = { theInsertCStr,nil };
const long	theLongInts[1] = { theNum };

	CopyCStrAndInsertCStrsLongIntsIntoCStr(theSrcCStr,theCStrs,theLongInts,theDstCStr,maxDstStrLength);
}



void CopyCStrAndInsertCStrsLongIntsIntoCStr(const char *theSrcCStr,const char **theInsertCStrs,const long *theLongInts,char *theDstCStr,const int maxDstStrLength)
{
int			dstCharIndex,srcCharIndex,theMaxDstStrLength;
int			theCStrIndex = 0;
int			theLongIntIndex = 0;

	
	theMaxDstStrLength = maxDstStrLength;
	
	if (theDstCStr != nil && theSrcCStr != nil && theMaxDstStrLength > 0)
	{
		dstCharIndex = 0;
		
		srcCharIndex = 0;
		
		
		//	Allow room for NULL at end of string
		
		theMaxDstStrLength--;
		
		
		for (;;)
		{
			//	Hit end of buffer?
			
			if (dstCharIndex >= theMaxDstStrLength)
			{
				theDstCStr[dstCharIndex++] = 0;
				
				goto EXITPOINT;
			}
			
			//	End of source string?
			
			else if (theSrcCStr[srcCharIndex] == 0)
			{
				theDstCStr[dstCharIndex++] = 0;
				
				goto EXITPOINT;
			}
			
			//	Did we find a '%s'?
			
			else if (theInsertCStrs != nil && theInsertCStrs[theCStrIndex] != nil && theSrcCStr[srcCharIndex] == '%' && theSrcCStr[srcCharIndex + 1] == 's')
			{
				//	Skip over the '%s'
				
				srcCharIndex += 2;
				
				
				//	Terminate the dest string and then concat the string
				
				theDstCStr[dstCharIndex] = 0;
				
				ConcatCStrToCStr(theInsertCStrs[theCStrIndex],theDstCStr,theMaxDstStrLength);
				
				dstCharIndex = CStrLength(theDstCStr);
				
				theCStrIndex++;
			}
			
			//	Did we find a '%ld'?
			
			else if (theLongInts != nil && theSrcCStr[srcCharIndex] == '%' && theSrcCStr[srcCharIndex + 1] == 'l' && theSrcCStr[srcCharIndex + 2] == 'd')
			{
				//	Skip over the '%ld'
				
				srcCharIndex += 3;
				
				
				//	Terminate the dest string and then concat the number
				
				theDstCStr[dstCharIndex] = 0;
				
				ConcatLongIntToCStr(theLongInts[theLongIntIndex],theDstCStr,theMaxDstStrLength);
				
				theLongIntIndex++;
				
				dstCharIndex = CStrLength(theDstCStr);
			}
			
			else
			{
				theDstCStr[dstCharIndex++] = theSrcCStr[srcCharIndex++];
			}
		}
	}



EXITPOINT:

	return;
}





OSErr CopyCStrAndInsertCStrLongIntIntoHandle(const char *theSrcCStr,const char *theInsertCStr,const long theNum,Handle *theHandle)
{
OSErr	errCode;
long	byteCount;

	
	if (theHandle != nil)
	{
		byteCount = CStrLength(theSrcCStr) + CStrLength(theInsertCStr) + 32;
		
		*theHandle = NewHandle(byteCount);
		
		if (*theHandle == nil)
		{
			SetErrorMessageAndLongIntAndBail("CopyCStrAndInsertCStrLongIntIntoHandle: Can't allocate Handle, MemError() = ",MemError());
		}
		
		
		HLock(*theHandle);
		
		CopyCStrAndInsertCStrLongIntIntoCStr(theSrcCStr,theInsertCStr,theNum,**theHandle,byteCount);
		
		HUnlock(*theHandle);
	}
	
	errCode = noErr;
	
	
EXITPOINT:

	return(errCode);
}





OSErr CopyIndexedWordToCStr(char *theSrcCStr,int whichWord,char *theDstCStr,int maxDstCStrLength)
{
OSErr		errCode;
char		*srcCharPtr,*dstCharPtr;
int			wordCount;
int			byteCount;


	if (theSrcCStr == nil)
	{
		SetErrorMessageAndBail(("CopyIndexedWordToCStr: Bad parameter, theSrcCStr == nil"));
	}
	
	if (theDstCStr == nil)
	{
		SetErrorMessageAndBail(("CopyIndexedWordToCStr: Bad parameter, theDstCStr == nil"));
	}
	
	if (whichWord < 0)
	{
		SetErrorMessageAndBail(("CopyIndexedWordToCStr: Bad parameter, whichWord < 0"));
	}
	
	if (maxDstCStrLength <= 0)
	{
		SetErrorMessageAndBail(("CopyIndexedWordToCStr: Bad parameter, maxDstCStrLength <= 0"));
	}

	
	*theDstCStr = '\0';
	
	srcCharPtr = theSrcCStr;

	while (*srcCharPtr == ' ' || *srcCharPtr == '\t')
	{
		srcCharPtr++;
	}
	

	for (wordCount = 0;wordCount < whichWord;wordCount++)
	{
		while (*srcCharPtr != ' ' && *srcCharPtr != '\t' && *srcCharPtr != '\r' && *srcCharPtr != '\n' && *srcCharPtr != '\0')
		{
			srcCharPtr++;
		}
		
		if (*srcCharPtr == '\r' || *srcCharPtr == '\n' || *srcCharPtr == '\0')
		{
			errCode = noErr;
			
			goto EXITPOINT;
		}

		while (*srcCharPtr == ' ' || *srcCharPtr == '\t')
		{
			srcCharPtr++;
		}
		
		if (*srcCharPtr == '\r' || *srcCharPtr == '\n' || *srcCharPtr == '\0')
		{
			errCode = noErr;
			
			goto EXITPOINT;
		}
	}


	dstCharPtr = theDstCStr;
	byteCount = 0;
	
	
	for(;;)
	{
		if (byteCount >= maxDstCStrLength - 1 || *srcCharPtr == '\0' || *srcCharPtr == ' ' || *srcCharPtr == '\t' || *srcCharPtr == '\r' || *srcCharPtr == '\n')
		{
			*dstCharPtr = '\0';
			break;
		}
		
		*dstCharPtr++ = *srcCharPtr++;
		
		byteCount++;
	}


	errCode = noErr;


EXITPOINT:

	return(errCode);
}





OSErr CopyIndexedWordToNewHandle(char *theSrcCStr,int whichWord,Handle *outTheHandle)
{
OSErr		errCode;
char		*srcCharPtr;
int			wordCount;
int			byteCount;


	if (theSrcCStr == nil)
	{
		SetErrorMessageAndBail(("CopyIndexedWordToNewHandle: Bad parameter, theSrcCStr == nil"));
	}
	
	if (outTheHandle == nil)
	{
		SetErrorMessageAndBail(("CopyIndexedWordToNewHandle: Bad parameter, outTheHandle == nil"));
	}
	
	if (whichWord < 0)
	{
		SetErrorMessageAndBail(("CopyIndexedWordToNewHandle: Bad parameter, whichWord < 0"));
	}

	
	*outTheHandle = nil;
	

	srcCharPtr = theSrcCStr;

	while (*srcCharPtr == ' ' || *srcCharPtr == '\t')
	{
		srcCharPtr++;
	}
	

	for (wordCount = 0;wordCount < whichWord;wordCount++)
	{
		while (*srcCharPtr != ' ' && *srcCharPtr != '\t' && *srcCharPtr != '\r' && *srcCharPtr != '\n' && *srcCharPtr != '\0')
		{
			srcCharPtr++;
		}
		
		if (*srcCharPtr == '\r' || *srcCharPtr == '\n' || *srcCharPtr == '\0')
		{
			break;
		}

		while (*srcCharPtr == ' ' || *srcCharPtr == '\t')
		{
			srcCharPtr++;
		}
		
		if (*srcCharPtr == '\r' || *srcCharPtr == '\n' || *srcCharPtr == '\0')
		{
			break;
		}
	}


	for (byteCount = 0;;byteCount++)
	{
		if (srcCharPtr[byteCount] == ' ' || srcCharPtr[byteCount] == '\t' || srcCharPtr[byteCount] == '\r' || srcCharPtr[byteCount] == '\n' || srcCharPtr[byteCount] == '\0')
		{
			break;
		}
	}

	
	*outTheHandle = NewHandle(byteCount + 1);
	
	if (*outTheHandle == nil)
	{
		SetErrorMessageAndLongIntAndBail("CopyIndexedWordToNewHandle: Can't allocate Handle, MemError() = ",MemError());
	}
	
	
	::BlockMove(srcCharPtr,**outTheHandle,byteCount);
	
	(**outTheHandle)[byteCount] = '\0';

	errCode = noErr;


EXITPOINT:

	return(errCode);
}



OSErr CopyIndexedLineToCStr(const char *theSrcCStr,int inWhichLine,int *lineEndIndex,Boolean *gotLastLine,char *theDstCStr,const int maxDstCStrLength)
{
OSErr		errCode;
int			theCurrentLine;
int			theCurrentLineOffset;
int			theEOSOffset;


	if (theSrcCStr == nil)
	{
		SetErrorMessageAndBail(("CopyIndexedLineToCStr: Bad parameter, theSrcCStr == nil"));
	}
	
	if (theDstCStr == nil)
	{
		SetErrorMessageAndBail(("CopyIndexedLineToCStr: Bad parameter, theDstCStr == nil"));
	}
	
	if (inWhichLine < 0)
	{
		SetErrorMessageAndBail(("CopyIndexedLineToCStr: Bad parameter, inWhichLine < 0"));
	}
	
	if (maxDstCStrLength <= 0)
	{
		SetErrorMessageAndBail(("CopyIndexedLineToCStr: Bad parameter, maxDstCStrLength <= 0"));
	}
	
	
	if (gotLastLine != nil)
	{
		*gotLastLine = false;
	}

	
	*theDstCStr = 0;
	
	theCurrentLineOffset = 0;
	
	theCurrentLine = 0;
	
	
	while (theCurrentLine < inWhichLine)
	{
		while (theSrcCStr[theCurrentLineOffset] != '\r' && theSrcCStr[theCurrentLineOffset] != 0)
		{
			theCurrentLineOffset++;
		}
		
		if (theSrcCStr[theCurrentLineOffset] == 0)
		{
			break;
		}
		
		theCurrentLineOffset++;
		theCurrentLine++;
	}
		
	if (theSrcCStr[theCurrentLineOffset] == 0)
	{
		SetErrorMessageAndLongIntAndBail("CopyIndexedLineToCStr: Too few lines in source text, can't get line ",inWhichLine);
	}


	theEOSOffset = FindCharOffsetInCStr('\r',theSrcCStr + theCurrentLineOffset);
	
	if (theEOSOffset >= 0)
	{
		CopyCSubstrToCStr(theSrcCStr + theCurrentLineOffset,theEOSOffset,theDstCStr,maxDstCStrLength);
		
		if (gotLastLine != nil)
		{
			*gotLastLine = false;
		}
	
		if (lineEndIndex != nil)
		{
			*lineEndIndex = theEOSOffset;
		}
	}
	
	else
	{
		theEOSOffset = CStrLength(theSrcCStr + theCurrentLineOffset);

		CopyCSubstrToCStr(theSrcCStr + theCurrentLineOffset,theEOSOffset,theDstCStr,maxDstCStrLength);
		
		if (gotLastLine != nil)
		{
			*gotLastLine = true;
		}
	
		if (lineEndIndex != nil)
		{
			*lineEndIndex = theEOSOffset;
		}
	}
	

	errCode = noErr;


EXITPOINT:

	return(errCode);
}



OSErr CopyIndexedLineToNewHandle(const char *theSrcCStr,int inWhichLine,Handle *outNewHandle)
{
OSErr		errCode;
int			theCurrentLine;
int			theCurrentLineOffset;
int			byteCount;


	SetErrorMessageAndBailIfNil(theSrcCStr,"CopyIndexedLineToNewHandle: Bad parameter, theSrcCStr == nil");
	SetErrorMessageAndBailIfNil(outNewHandle,"CopyIndexedLineToNewHandle: Bad parameter, outNewHandle == nil");
	
	if (inWhichLine < 0)
	{
		SetErrorMessageAndBail(("CopyIndexedLineToNewHandle: Bad parameter, inWhichLine < 0"));
	}
	

	theCurrentLineOffset = 0;
	
	theCurrentLine = 0;
	
	
	while (theCurrentLine < inWhichLine)
	{
		while (theSrcCStr[theCurrentLineOffset] != '\r' && theSrcCStr[theCurrentLineOffset] != '\0')
		{
			theCurrentLineOffset++;
		}
		
		if (theSrcCStr[theCurrentLineOffset] == '\0')
		{
			break;
		}
		
		theCurrentLineOffset++;
		theCurrentLine++;
	}
		
	if (theSrcCStr[theCurrentLineOffset] == '\0')
	{
		SetErrorMessageAndLongIntAndBail("CopyIndexedLineToNewHandle: Too few lines in source text, can't get line #",inWhichLine);
	}

	
	byteCount = 0;
	
	while (theSrcCStr[theCurrentLineOffset + byteCount] != '\r' && theSrcCStr[theCurrentLineOffset + byteCount] != '\0')
	{
		byteCount++;
	}
		
	
	*outNewHandle = NewHandle(byteCount + 1);
	
	if (*outNewHandle == nil)
	{
		SetErrorMessageAndLongIntAndBail("CopyIndexedLineToNewHandle: Can't allocate Handle, MemError() = ",MemError());
	}
	
	::BlockMove(theSrcCStr + theCurrentLineOffset,**outNewHandle,byteCount);
	
	(**outNewHandle)[byteCount] = '\0';

	errCode = noErr;


EXITPOINT:

	return(errCode);
}




OSErr CountDigits(const char *inCStr,int *outNumIntegerDigits,int *outNumFractDigits)
{
OSErr	errCode = noErr;
int		numIntDigits = 0;
int		numFractDigits = 0;
int 	digitIndex = 0;

	
	SetErrorMessageAndBailIfNil(inCStr,"CountDigits: Bad parameter, theSrcCStr == nil");
	SetErrorMessageAndBailIfNil(outNumIntegerDigits,"CountDigits: Bad parameter, outNumIntegerDigits == nil");
	SetErrorMessageAndBailIfNil(outNumFractDigits,"CountDigits: Bad parameter, outNumFractDigits == nil");
	
	digitIndex = 0;
	
	while (inCStr[digitIndex] >= '0' && inCStr[digitIndex] <= '9')
	{
		digitIndex++;
		numIntDigits++;
	}
	
	if (inCStr[digitIndex] == '.')
	{
		digitIndex++;
		
		while (inCStr[digitIndex] >= '0' && inCStr[digitIndex] <= '9')
		{
			digitIndex++;
			numFractDigits++;
		}
	}
	
	*outNumIntegerDigits = numIntDigits;
	
	*outNumFractDigits = numFractDigits;
	
	errCode = noErr;
	
EXITPOINT:

	return(errCode);
}



OSErr ExtractIntFromCStr(const char *theSrcCStr,int *outInt,Boolean skipLeadingSpaces)
{
OSErr		errCode;
int			theCharIndex;


	if (theSrcCStr == nil)
	{
		SetErrorMessageAndBail(("ExtractIntFromCStr: Bad parameter, theSrcCStr == nil"));
	}
	
	if (outInt == nil)
	{
		SetErrorMessageAndBail(("ExtractIntFromCStr: Bad parameter, outInt == nil"));
	}	

	
	*outInt = 0;
	
	theCharIndex = 0;
	
	if (skipLeadingSpaces == true)
	{
		while (theSrcCStr[theCharIndex] == ' ')
		{
			theCharIndex++;
		}
	}
	
	if (theSrcCStr[theCharIndex] < '0' || theSrcCStr[theCharIndex] > '9')
	{
		SetErrorMessageAndBail(("ExtractIntFromCStr: Bad parameter, theSrcCStr contains a bogus numeric representation"));
	}


	while (theSrcCStr[theCharIndex] >= '0' && theSrcCStr[theCharIndex] <= '9')
	{
		*outInt = (*outInt * 10) + (theSrcCStr[theCharIndex] - '0');
		
		theCharIndex++;
	}
	

	errCode = noErr;


EXITPOINT:

	return(errCode);
}



OSErr ExtractIntFromPStr(const unsigned char *theSrcPStr,int *outInt,Boolean skipLeadingSpaces)
{
OSErr		errCode;
char		theCStr[256];


	if (theSrcPStr == nil)
	{
		SetErrorMessageAndBail(("ExtractIntFromPStr: Bad parameter, theSrcPStr == nil"));
	}
	
	if (outInt == nil)
	{
		SetErrorMessageAndBail(("ExtractIntFromPStr: Bad parameter, outInt == nil"));
	}
	
	
	CopyPStrToCStr(theSrcPStr,theCStr,sizeof(theCStr));
	
	
	errCode = ExtractIntFromCStr(theCStr,outInt,skipLeadingSpaces);


EXITPOINT:

	return(errCode);
}



int CountOccurencesOfCharInCStr(const char inChar,const char *inSrcCStr)
{
int		theSrcCharIndex;
int		numOccurrences = -1;


	if (inSrcCStr != nil && inChar != '\0')
	{
		numOccurrences = 0;
		
		for (theSrcCharIndex = 0;inSrcCStr[theSrcCharIndex] != '\0';theSrcCharIndex++)
		{
			if (inSrcCStr[theSrcCharIndex] == inChar)
			{
				numOccurrences++;
			}
		}
	}
	
	return(numOccurrences);
}


int CountWordsInCStr(const char *inSrcCStr)
{
int		numWords = -1;


	if (inSrcCStr != nil)
	{
		numWords = 0;
		
		//	Skip lead spaces
		
		while (*inSrcCStr == ' ')
		{
			inSrcCStr++;
		}

		while (*inSrcCStr != '\0')
		{
			numWords++;

			while (*inSrcCStr != ' ' && *inSrcCStr != '\0')
			{
				inSrcCStr++;
			}
			
			while (*inSrcCStr == ' ')
			{
				inSrcCStr++;
			}
		}
	}
	
	return(numWords);
}




void ConvertCStrToUpperCase(char *theSrcCStr)
{
char		*theCharPtr;


	if (theSrcCStr != nil)
	{
		theCharPtr = theSrcCStr;
		
		while (*theCharPtr != 0)
		{
			if (*theCharPtr >= 'a' && *theCharPtr <= 'z')
			{
				*theCharPtr = *theCharPtr - 'a' + 'A';
			}
			
			theCharPtr++;
		}
	}
}







void ExtractCStrItemFromCStr(const char *inSrcCStr,const char inItemDelimiter,const int inItemNumber,Boolean *foundItem,char *outDstCharPtr,const int inDstCharPtrMaxLength,const Boolean inTreatMultipleDelimsAsSingleDelim)
{
int		theItem;
int		theSrcCharIndex;
int		theDstCharIndex;


	if (foundItem != nil)
	{
		*foundItem = false;
	}
	
	
	if (outDstCharPtr != nil && inDstCharPtrMaxLength > 0 && inItemNumber >= 0 && inItemDelimiter != 0)
	{
		*outDstCharPtr = 0;
		

		theSrcCharIndex = 0;
		
		for (theItem = 0;theItem < inItemNumber;theItem++)
		{
			while (inSrcCStr[theSrcCharIndex] != inItemDelimiter && inSrcCStr[theSrcCharIndex] != '\0')
			{
				theSrcCharIndex++;
			}
			
			if (inSrcCStr[theSrcCharIndex] == inItemDelimiter)
			{
				theSrcCharIndex++;
				
				if (inTreatMultipleDelimsAsSingleDelim)
				{
					while (inSrcCStr[theSrcCharIndex] == inItemDelimiter)
					{
						theSrcCharIndex++;
					}
				}
			}
			
			
			if (inSrcCStr[theSrcCharIndex] == '\0')
			{
				goto EXITPOINT;
			}
		}
		

		if (foundItem != nil)
		{
			*foundItem = true;
		}
		
		
		theDstCharIndex = 0;
		
		for (;;)
		{
			if (inSrcCStr[theSrcCharIndex] == 0 || inSrcCStr[theSrcCharIndex] == inItemDelimiter || theDstCharIndex >= inDstCharPtrMaxLength - 1)
			{
				outDstCharPtr[theDstCharIndex] = 0;
				
				break;
			}
			
			outDstCharPtr[theDstCharIndex++] = inSrcCStr[theSrcCharIndex++];
		}
	}
	
	
EXITPOINT:

	return;
}



OSErr ExtractCStrItemFromCStrIntoNewHandle(const char *inSrcCStr,const char inItemDelimiter,const int inItemNumber,Boolean *foundItem,Handle *outNewHandle,const Boolean inTreatMultipleDelimsAsSingleDelim)
{
OSErr	errCode;
int		theItem;
int		theSrcCharIndex;
int		theItemLength;


	if (inSrcCStr == nil)
	{
		SetErrorMessage("ExtractCStrItemFromCStrIntoNewHandle: Bad parameter, inSrcCStr == nil");
		errCode = kGenericError;
		goto EXITPOINT;
	}
	
	if (outNewHandle == nil)
	{
		SetErrorMessage("ExtractCStrItemFromCStrIntoNewHandle: Bad parameter, outNewHandle == nil");
		errCode = kGenericError;
		goto EXITPOINT;
	}
	
	if (foundItem == nil)
	{
		SetErrorMessage("ExtractCStrItemFromCStrIntoNewHandle: Bad parameter, foundItem == nil");
		errCode = kGenericError;
		goto EXITPOINT;
	}
	
	if (inItemNumber < 0)
	{
		SetErrorMessage("ExtractCStrItemFromCStrIntoNewHandle: Bad parameter, inItemNumber < 0");
		errCode = kGenericError;
		goto EXITPOINT;
	}
	
	if (inItemDelimiter == 0)
	{
		SetErrorMessage("ExtractCStrItemFromCStrIntoNewHandle: Bad parameter, inItemDelimiter == 0");
		errCode = kGenericError;
		goto EXITPOINT;
	}


	*foundItem = false;
	
	theSrcCharIndex = 0;
	
	for (theItem = 0;theItem < inItemNumber;theItem++)
	{
		while (inSrcCStr[theSrcCharIndex] != inItemDelimiter && inSrcCStr[theSrcCharIndex] != '\0')
		{
			theSrcCharIndex++;
		}
		
		if (inSrcCStr[theSrcCharIndex] == inItemDelimiter)
		{
			theSrcCharIndex++;
			
			if (inTreatMultipleDelimsAsSingleDelim)
			{
				while (inSrcCStr[theSrcCharIndex] == inItemDelimiter)
				{
					theSrcCharIndex++;
				}
			}
		}
		
		
		if (inSrcCStr[theSrcCharIndex] == '\0')
		{
			errCode = noErr;
			
			goto EXITPOINT;
		}
	}
	

	*foundItem = true;
	
	
	for (theItemLength = 0;;theItemLength++)
	{
		if (inSrcCStr[theSrcCharIndex + theItemLength] == 0 || inSrcCStr[theSrcCharIndex + theItemLength] == inItemDelimiter)
		{
			break;
		}
	}
	

	*outNewHandle = NewHandle(theItemLength + 1);
	
	if (*outNewHandle == nil)
	{
		SetErrorMessageAndLongIntAndBail("ExtractCStrItemFromCStrIntoNewHandle: Can't allocate Handle, MemError() = ",MemError());
	}
	
	
	BlockMove(inSrcCStr + theSrcCharIndex,**outNewHandle,theItemLength);
	
	(**outNewHandle)[theItemLength] = 0;
	
	errCode = noErr;
	
	
EXITPOINT:

	return(errCode);
}






OSErr ExtractFloatFromCStr(const char *inCString,extended80 *outFloat)
{
OSErr				errCode;
Str255				theStr255;
Handle				theNumberPartsTableHandle = nil;
long				theNumberPartsOffset,theNumberPartsLength;
FormatResultType	theFormatResultType;
NumberParts			theNumberPartsTable;
NumFormatStringRec	theNumFormatStringRec;


	if (inCString == nil)
	{
		SetErrorMessage("ExtractFloatFromCStr: Bad parameter, inCString == nil");
		errCode = kGenericError;
		goto EXITPOINT;
	}

	if (outFloat == nil)
	{
		SetErrorMessage("ExtractFloatFromCStr: Bad parameter, outFloat == nil");
		errCode = kGenericError;
		goto EXITPOINT;
	}
	
	
//	GetIntlResourceTable(smRoman,smNumberPartsTable,&theNumberPartsTableHandle,&theNumberPartsOffset,&theNumberPartsLength);

	GetIntlResourceTable(GetScriptManagerVariable(smSysScript),smNumberPartsTable,&theNumberPartsTableHandle,&theNumberPartsOffset,&theNumberPartsLength);	
	
	if (theNumberPartsTableHandle == nil)
	{
		SetErrorMessage("ExtractFloatFromCStr: Can't get number parts table for converting string representations to/from numeric representations");
		errCode = kGenericError;
		goto EXITPOINT;
	}
	
	if (theNumberPartsLength > sizeof(theNumberPartsTable))
	{
		SetErrorMessage("ExtractFloatFromCStr: Number parts table has bad length");
		errCode = kGenericError;
		goto EXITPOINT;
	}
	

	BlockMove(*theNumberPartsTableHandle + theNumberPartsOffset,&theNumberPartsTable,theNumberPartsLength);
	
	
	theFormatResultType = (FormatResultType) StringToFormatRec(kNumberFormatString,&theNumberPartsTable,&theNumFormatStringRec);
	
	if (theFormatResultType != fFormatOK)
	{
		SetErrorMessage("ExtractFloatFromCStr: StringToFormatRec() != fFormatOK");
		errCode = kGenericError;
		goto EXITPOINT;
	}

	
	CopyCStrToPStr(inCString,theStr255,sizeof(theStr255));


	theFormatResultType = (FormatResultType) StringToExtended(theStr255,&theNumFormatStringRec,&theNumberPartsTable,outFloat);
	
	if (theFormatResultType != fFormatOK && theFormatResultType != fBestGuess)
	{
		SetErrorMessageAndLongIntAndBail("ExtractFloatFromCStr: StringToExtended() = ",theFormatResultType);
	}

	
	errCode = noErr;
	

EXITPOINT:
	
	return(errCode);
}



OSErr CopyFloatToCStr(const extended80 *theFloat,char *theCStr,const int maxCStrLength,const int inMaxNumIntDigits,const int inMaxNumFractDigits)
{
OSErr				errCode;
Str255				theStr255;
Handle				theNumberPartsTableHandle = nil;
long				theNumberPartsOffset,theNumberPartsLength;
FormatResultType	theFormatResultType;
NumberParts			theNumberPartsTable;
NumFormatStringRec	theNumFormatStringRec;


	if (theCStr == nil)
	{
		SetErrorMessage("CopyFloatToCStr: Bad parameter, theCStr == nil");
		errCode = kGenericError;
		goto EXITPOINT;
	}

	if (theFloat == nil)
	{
		SetErrorMessage("CopyFloatToCStr: Bad parameter, theFloat == nil");
		errCode = kGenericError;
		goto EXITPOINT;
	}
	

//	GetIntlResourceTable(smRoman,smNumberPartsTable,&theNumberPartsTableHandle,&theNumberPartsOffset,&theNumberPartsLength);

	GetIntlResourceTable(GetScriptManagerVariable(smSysScript),smNumberPartsTable,&theNumberPartsTableHandle,&theNumberPartsOffset,&theNumberPartsLength);	
	
	if (theNumberPartsTableHandle == nil)
	{
		SetErrorMessage("CopyFloatToCStr: Can't get number parts table for converting string representations to/from numeric representations");
		errCode = kGenericError;
		goto EXITPOINT;
	}
	
	if (theNumberPartsLength > sizeof(theNumberPartsTable))
	{
		SetErrorMessage("CopyFloatToCStr: Number parts table has bad length");
		errCode = kGenericError;
		goto EXITPOINT;
	}
	
	
	BlockMove(*theNumberPartsTableHandle + theNumberPartsOffset,&theNumberPartsTable,theNumberPartsLength);
	
	
	if (inMaxNumIntDigits >= 0 || inMaxNumFractDigits >= 0)
	{
	char	numberFormat[64];
	int		numberFormatLength = 0;
	
		for (int i = 0;i < inMaxNumIntDigits && numberFormatLength < sizeof(numberFormat) - 1;i++)
		{
			numberFormat[numberFormatLength++] = '0';
		}
		
		if (inMaxNumFractDigits > 0 && numberFormatLength < sizeof(numberFormat) - 1)
		{
			numberFormat[numberFormatLength++] = '.';
			
			for (int i = 0;i < inMaxNumFractDigits && numberFormatLength < sizeof(numberFormat) - 1;i++)
			{
				numberFormat[numberFormatLength++] = '0';
			}
		}

		
		if (numberFormatLength < sizeof(numberFormat) - 1)
		{
			numberFormat[numberFormatLength++] = ';';
		}
		
		if (numberFormatLength < sizeof(numberFormat) - 1)
		{
			numberFormat[numberFormatLength++] = '-';
		}
		

		for (int i = 0;i < inMaxNumIntDigits && numberFormatLength < sizeof(numberFormat) - 1;i++)
		{
			numberFormat[numberFormatLength++] = '0';
		}
		
		if (inMaxNumFractDigits > 0 && numberFormatLength < sizeof(numberFormat) - 1)
		{
			numberFormat[numberFormatLength++] = '.';
			
			for (int i = 0;i < inMaxNumFractDigits && numberFormatLength < sizeof(numberFormat) - 1;i++)
			{
				numberFormat[numberFormatLength++] = '0';
			}
		}
		
		numberFormat[numberFormatLength] = '\0';


	Str255	tempStr255;
	
		CopyCStrToPStr(numberFormat,tempStr255,sizeof(tempStr255));
		
		theFormatResultType = (FormatResultType) StringToFormatRec(tempStr255,&theNumberPartsTable,&theNumFormatStringRec);
	}
	
	else
	{
		theFormatResultType = (FormatResultType) StringToFormatRec(kNumberFormatString,&theNumberPartsTable,&theNumFormatStringRec);
	}
	
	if (theFormatResultType != fFormatOK)
	{
		SetErrorMessage("CopyFloatToCStr: StringToFormatRec() != fFormatOK");
		errCode = kGenericError;
		goto EXITPOINT;
	}


	theFormatResultType = (FormatResultType) ExtendedToString(theFloat,&theNumFormatStringRec,&theNumberPartsTable,theStr255);
	
	if (theFormatResultType != fFormatOK)
	{
		SetErrorMessage("CopyFloatToCStr: ExtendedToString() != fFormatOK");
		errCode = kGenericError;
		goto EXITPOINT;
	}

	
	CopyPStrToCStr(theStr255,theCStr,maxCStrLength);
	
	errCode = noErr;
	

EXITPOINT:
	
	return(errCode);
}





void SkipWhiteSpace(char **ioSrcCharPtr,const Boolean inStopAtEOL)
{
	if (ioSrcCharPtr != nil && *ioSrcCharPtr != nil)
	{
		if (inStopAtEOL)
		{
			while ((**ioSrcCharPtr == ' ' || **ioSrcCharPtr == '\t') && **ioSrcCharPtr != '\r' && **ioSrcCharPtr != '\n')
			{
				*ioSrcCharPtr++;
			}
		}
		
		else
		{
			while (**ioSrcCharPtr == ' ' || **ioSrcCharPtr == '\t')
			{
				*ioSrcCharPtr++;
			}
		}
	}
}
