/*
 *	A simple socket-like package.  
 *	This could undoubtedly be improved, since it does polling and busy-waiting.  
 *	At least it uses asynch I/O and implements timeouts!
 *
 *	Other funkiness includes the use of my own (possibly brain-damaged) error-handling infrastructure.
 *
 *	-Roy Wood (roy@centricsystems.ca)
 *
 */


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
 




#include "MacSocket.h"

#include <Threads.h>

#include <OpenTransport.h>
#include <OpenTpTInternet.h>
#include <OpenTptClient.h>



#include "CPStringUtils.hpp"
#include "ErrorHandling.hpp"


//	#define MACSOCKET_DEBUG		1

#ifdef MACSOCKET_DEBUG
	#include <stdio.h>
#endif



extern int errno;


#define kMaxNumSockets			4


struct SocketStruct
{
	Boolean						mIsInUse;

	Boolean						mEndpointIsBound;

	Boolean						mLocalEndIsConnected;
	Boolean						mRemoteEndIsConnected;

	Boolean						mReceivedTOpenComplete;
	Boolean						mReceivedTBindComplete;
	Boolean						mReceivedTConnect;
	Boolean						mReceivedTListen;
	Boolean						mReceivedTPassCon;
	Boolean						mReceivedTDisconnect;
	Boolean						mReceivedTOrdRel;
	Boolean						mReceivedTDisconnectComplete;
	
	long						mTimeoutTicks;
	long						mOperationStartTicks;
	
	MacSocket_IdleWaitCallback	mIdleWaitCallback;
	void						*mUserRefPtr;
	
	OTEventCode					mExpectedCode;
	OTResult					mAsyncOperationResult;
	
	EndpointRef		 			mEndPointRef;
	TBind						*mBindRequestedAddrInfo;
	TBind						*mAssignedAddrInfo;
	TCall						*mRemoteAddrInfo;
	
	Boolean						mReadyToReadData;
	Boolean						mReadyToWriteData;
	
	Ptr							mReadBuffer;
	Ptr							mWriteBuffer;
	
	int							mLastError;
	char						mErrMessage[256];
};

typedef struct SocketStruct	SocketStruct;


static SocketStruct			sSockets[kMaxNumSockets];
static Boolean				sSocketsSetup = false;




static OSErr MyBusyWait(SocketStruct *ioSocket,Boolean returnImmediatelyOnError,OTResult *outOTResult,Boolean *inAsyncOperationCompleteFlag);

static pascal void OTNonYieldingNotifier(void *contextPtr,OTEventCode code,OTResult result,void *cookie);

static Boolean	SocketIndexIsValid(const int inSocketNum);

static void InitSocket(SocketStruct *ioSocket);

static void PrepareForAsyncOperation(SocketStruct *ioSocket,const OTEventCode inExpectedCode);

static Boolean TimeoutElapsed(const SocketStruct *inSocket);

static OSStatus NegotiateIPReuseAddrOption(EndpointRef inEndpoint,const Boolean inEnableReuseIP);



void MacSocket_GetSocketErrorInfo(const int inSocketNum,int *outSocketErrCode,char *outSocketErrString,const int inSocketErrStringMaxLength)
{
	if (outSocketErrCode != nil)
	{
		*outSocketErrCode = -1;
	}
	
	if (outSocketErrString != nil)
	{
		CopyCStrToCStr("",outSocketErrString,inSocketErrStringMaxLength);
	}
	
	
	if (SocketIndexIsValid(inSocketNum))
	{
	SocketStruct	*theSocketStruct = &(sSockets[inSocketNum]);
	
		
		if (outSocketErrCode != nil)
		{
			*outSocketErrCode = theSocketStruct->mLastError;
		}

		if (outSocketErrString != nil)
		{
			CopyCStrToCStr(theSocketStruct->mErrMessage,outSocketErrString,inSocketErrStringMaxLength);
		}
	}
}


void MacSocket_SetUserRefPtr(const int inSocketNum,void *inNewRefPtr)
{
	if (SocketIndexIsValid(inSocketNum))
	{
	SocketStruct	*theSocketStruct = &(sSockets[inSocketNum]);

		theSocketStruct->mUserRefPtr = inNewRefPtr;
	}
}



void MacSocket_GetLocalIPAndPort(const int inSocketNum,char *outIPAndPort,const int inIPAndPortLength)
{
	if (outIPAndPort != nil && SocketIndexIsValid(inSocketNum))
	{
	char			tempString[256];
	SocketStruct	*theSocketStruct = &(sSockets[inSocketNum]);
	
		
		CopyCStrToCStr("",tempString,sizeof(tempString));

		if (theSocketStruct->mAssignedAddrInfo != nil)
		{
		InetAddress		*theInetAddress = (InetAddress *) theSocketStruct->mAssignedAddrInfo->addr.buf;
		InetHost		theInetHost = theInetAddress->fHost;
			
			if (theInetHost == 0)
			{
			InetInterfaceInfo	theInetInterfaceInfo;
				
				if (::OTInetGetInterfaceInfo(&theInetInterfaceInfo,kDefaultInetInterface) == noErr)
				{
					theInetHost = theInetInterfaceInfo.fAddress;
				}
			}
		
			::OTInetHostToString(theInetHost,tempString);
			
			ConcatCStrToCStr(":",tempString,sizeof(tempString));
			ConcatLongIntToCStr(theInetAddress->fPort,tempString,sizeof(tempString));
		}
		
		CopyCStrToCStr(tempString,outIPAndPort,inIPAndPortLength);
	}
}



void MacSocket_GetRemoteIPAndPort(const int inSocketNum,char *outIPAndPort,const int inIPAndPortLength)
{
	if (outIPAndPort != nil && SocketIndexIsValid(inSocketNum))
	{
	char			tempString[256];
	SocketStruct	*theSocketStruct = &(sSockets[inSocketNum]);
	
		
		CopyCStrToCStr("",tempString,sizeof(tempString));

		if (theSocketStruct->mRemoteAddrInfo != nil)
		{
		InetAddress		*theInetAddress = (InetAddress *) theSocketStruct->mRemoteAddrInfo->addr.buf;
		InetHost		theInetHost = theInetAddress->fHost;
			
			if (theInetHost == 0)
			{
			InetInterfaceInfo	theInetInterfaceInfo;
				
				if (::OTInetGetInterfaceInfo(&theInetInterfaceInfo,kDefaultInetInterface) == noErr)
				{
					theInetHost = theInetInterfaceInfo.fAddress;
				}
			}
		
			::OTInetHostToString(theInetHost,tempString);
			
			ConcatCStrToCStr(":",tempString,sizeof(tempString));
			ConcatLongIntToCStr(theInetAddress->fPort,tempString,sizeof(tempString));
		}
		
		CopyCStrToCStr(tempString,outIPAndPort,inIPAndPortLength);
	}
}



Boolean MacSocket_RemoteEndIsClosing(const int inSocketNum)
{
Boolean		theResult = false;

	if (SocketIndexIsValid(inSocketNum))
	{
	SocketStruct	*theSocketStruct = &(sSockets[inSocketNum]);

		theResult = theSocketStruct->mReceivedTOrdRel;
	}

	return(theResult);
}



Boolean MacSocket_ListenCompleted(const int inSocketNum)
{
Boolean		theResult = false;

	if (SocketIndexIsValid(inSocketNum))
	{
	SocketStruct	*theSocketStruct = &(sSockets[inSocketNum]);

		theResult = theSocketStruct->mReceivedTPassCon;
	}

	return(theResult);
}



Boolean MacSocket_RemoteEndIsOpen(const int inSocketNum)
{
	if (SocketIndexIsValid(inSocketNum))
	{
	SocketStruct	*theSocketStruct = &(sSockets[inSocketNum]);
	
		return(theSocketStruct->mRemoteEndIsConnected);
	}
	
	else
	{
		return(false);
	}
}



Boolean MacSocket_LocalEndIsOpen(const int inSocketNum)
{
	if (SocketIndexIsValid(inSocketNum))
	{
	SocketStruct	*theSocketStruct = &(sSockets[inSocketNum]);
	
		return(theSocketStruct->mLocalEndIsConnected);
	}
	
	else
	{
		return(false);
	}
}



static Boolean TimeoutElapsed(const SocketStruct *inSocket)
{
Boolean		timeIsUp = false;

	if (inSocket != nil && inSocket->mTimeoutTicks > 0 && ::TickCount() > inSocket->mOperationStartTicks + inSocket->mTimeoutTicks)
	{
		timeIsUp = true;
	}
	
	
	return(timeIsUp);
}



static Boolean SocketIndexIsValid(const int inSocketNum)
{
	if (inSocketNum >= 0 && inSocketNum < kMaxNumSockets && sSockets[inSocketNum].mEndPointRef != kOTInvalidEndpointRef)
	{
		return(true);
	}
	
	else
	{
		return(false);
	}
}



static void InitSocket(SocketStruct *ioSocket)
{
	ioSocket->mIsInUse = false;
	
	ioSocket->mEndpointIsBound = false;
	
	ioSocket->mLocalEndIsConnected = false;
	ioSocket->mRemoteEndIsConnected = false;
	
	ioSocket->mReceivedTOpenComplete = false;
	ioSocket->mReceivedTBindComplete = false;
	ioSocket->mReceivedTConnect = false;
	ioSocket->mReceivedTListen = false;
	ioSocket->mReceivedTPassCon = false;
	ioSocket->mReceivedTDisconnect = false;
	ioSocket->mReceivedTOrdRel = false;
	ioSocket->mReceivedTDisconnectComplete = false;
	
	ioSocket->mTimeoutTicks = 30 * 60;
	ioSocket->mOperationStartTicks = -1;
	
	ioSocket->mIdleWaitCallback = nil;
	ioSocket->mUserRefPtr = nil;
	
	ioSocket->mExpectedCode = 0;
	ioSocket->mAsyncOperationResult = noErr;
	
	ioSocket->mEndPointRef = kOTInvalidEndpointRef;
	
	ioSocket->mBindRequestedAddrInfo = nil;
	ioSocket->mAssignedAddrInfo = nil;
	ioSocket->mRemoteAddrInfo = nil;
	
	ioSocket->mReadyToReadData = false;
	ioSocket->mReadyToWriteData = true;
	
	ioSocket->mReadBuffer = nil;
	ioSocket->mWriteBuffer = nil;

	ioSocket->mLastError = noErr;
	CopyCStrToCStr("",ioSocket->mErrMessage,sizeof(ioSocket->mErrMessage));
}



static void PrepareForAsyncOperation(SocketStruct *ioSocket,const OTEventCode inExpectedCode)
{
	ioSocket->mOperationStartTicks = ::TickCount();
	
	ioSocket->mAsyncOperationResult = noErr;
	
	ioSocket->mExpectedCode = inExpectedCode;
}


//	The wait function....

static OSErr MyBusyWait(SocketStruct *ioSocket,Boolean returnImmediatelyOnError,OTResult *outOTResult,Boolean *inAsyncOperationCompleteFlag)
{
OSErr 		errCode = noErr;
OTResult	theOTResult = noErr;

	
	SetErrorMessageAndBailIfNil(ioSocket,"MyBusyWait: Bad parameter, ioSocket = nil");
	SetErrorMessageAndBailIfNil(inAsyncOperationCompleteFlag,"MyBusyWait: Bad parameter, inAsyncOperationCompleteFlag = nil");
	
	for (;;) 
	{
		if (*inAsyncOperationCompleteFlag)
		{
			theOTResult = ioSocket->mAsyncOperationResult;
			
			break;
		}
		
		if (ioSocket->mIdleWaitCallback != nil)
		{
			theOTResult = (*(ioSocket->mIdleWaitCallback))(ioSocket->mUserRefPtr);
			
			if (theOTResult != noErr && returnImmediatelyOnError)
			{
				break;
			}
		}
		
		if (TimeoutElapsed(ioSocket))
		{
			theOTResult = kMacSocket_TimeoutErr;
			
			break;
		}
	}


EXITPOINT:
	
	if (outOTResult != nil)
	{
		*outOTResult = theOTResult;
	}
	
	return(errCode);
}



//	I used to do thread switching, but stopped.  It could easily be rolled back in though....

static pascal void OTNonYieldingNotifier(void *contextPtr,OTEventCode code,OTResult result,void *cookie)
{
SocketStruct *theSocketStruct = (SocketStruct *) contextPtr;
	
	if (theSocketStruct != nil)
	{
		if (theSocketStruct->mExpectedCode != 0 && code == theSocketStruct->mExpectedCode)
		{
			theSocketStruct->mAsyncOperationResult = result;
			
			theSocketStruct->mExpectedCode = 0;
		}
		
		
		switch (code) 
		{
			case T_OPENCOMPLETE:
			{
				theSocketStruct->mReceivedTOpenComplete = true;
				
				theSocketStruct->mEndPointRef = (EndpointRef) cookie;
				
				break;
			}

			
			case T_BINDCOMPLETE:
			{
				theSocketStruct->mReceivedTBindComplete = true;
				
				break;
			}
			

			case T_CONNECT:
			{
				theSocketStruct->mReceivedTConnect = true;

				theSocketStruct->mLocalEndIsConnected = true;
				
				theSocketStruct->mRemoteEndIsConnected = true;

				break;
			}
			

			case T_LISTEN:
			{
				theSocketStruct->mReceivedTListen = true;
				
				break;
			}
			

			case T_PASSCON:
			{
				theSocketStruct->mReceivedTPassCon = true;
				
				theSocketStruct->mLocalEndIsConnected = true;
				
				theSocketStruct->mRemoteEndIsConnected = true;

				break;
			}


			case T_DATA:
			{
				theSocketStruct->mReadyToReadData = true;
				
				break;
			}
			
			case T_GODATA:
			{
				theSocketStruct->mReadyToWriteData = true;
				
				break;
			}
			
			case T_DISCONNECT:
			{
				theSocketStruct->mReceivedTDisconnect = true;
				
				theSocketStruct->mRemoteEndIsConnected = false;
				
				theSocketStruct->mLocalEndIsConnected = false;
				
				::OTRcvDisconnect(theSocketStruct->mEndPointRef,nil);
				
				break;
			}

			case T_ORDREL:
			{
				theSocketStruct->mReceivedTOrdRel = true;
				
				//	We can still write data, so don't clear mRemoteEndIsConnected
				
				::OTRcvOrderlyDisconnect(theSocketStruct->mEndPointRef);
				
				break;
			}
			
			case T_DISCONNECTCOMPLETE:
			{
				theSocketStruct->mReceivedTDisconnectComplete = true;
				
				theSocketStruct->mRemoteEndIsConnected = false;
				
				theSocketStruct->mLocalEndIsConnected = false;
				
				break;
			}
		}
	}
/*
T_LISTEN OTListen
T_CONNECT OTRcvConnect
T_DATA OTRcv, OTRcvUData
T_DISCONNECT OTRcvDisconnect
T_ORDREL OTRcvOrderlyDisconnect
T_GODATA OTSnd, OTSndUData, OTLook
T_PASSCON none

T_EXDATA OTRcv
T_GOEXDATA OTSnd, OTLook
T_UDERR OTRcvUDErr
*/
}



//	Initialize the main socket data structure

OSErr MacSocket_Startup(void)
{
	if (!sSocketsSetup)
	{
		for (int i = 0;i < kMaxNumSockets;i++)
		{
			InitSocket(&(sSockets[i]));
		}

		::InitOpenTransport();
		
		sSocketsSetup = true;
	}
	
	
	return(noErr);
}



//	Cleanup before exiting

OSErr MacSocket_Shutdown(void)
{
	if (sSocketsSetup)
	{
		for (int i = 0;i < kMaxNumSockets;i++)
		{
		SocketStruct *theSocketStruct = &(sSockets[i]);
		
			if (theSocketStruct->mIsInUse)
			{
				if (theSocketStruct->mEndPointRef != kOTInvalidEndpointRef)
				{
				OTResult	theOTResult;
				
				
					//	Since we're killing the endpoint, I don't bother to send the disconnect (sorry!)

/*
					if (theSocketStruct->mLocalEndIsConnected)
					{
						//	This is an abortive action, so we do a hard disconnect instead of an OTSndOrderlyDisconnect
						
						theOTResult = ::OTSndDisconnect(theSocketStruct->mEndPointRef, nil);
						
						//	Now we have to watch for T_DISCONNECTCOMPLETE event
						
						theSocketStruct->mLocalEndIsConnected = false;
					}
*/					
					
					theOTResult = ::OTCloseProvider(theSocketStruct->mEndPointRef);
					
					
					theSocketStruct->mEndPointRef = kOTInvalidEndpointRef;
				}
				
				if (theSocketStruct->mBindRequestedAddrInfo != nil)
				{
					::OTFree((void *) theSocketStruct->mBindRequestedAddrInfo,T_BIND);
					
					theSocketStruct->mBindRequestedAddrInfo = nil;
				}
				
				if (theSocketStruct->mAssignedAddrInfo != nil)
				{
					::OTFree((void *) theSocketStruct->mAssignedAddrInfo,T_BIND);
					
					theSocketStruct->mAssignedAddrInfo = nil;
				}
				
				if (theSocketStruct->mRemoteAddrInfo != nil)
				{
					::OTFree((void *) theSocketStruct->mRemoteAddrInfo,T_CALL);
					
					theSocketStruct->mRemoteAddrInfo = nil;
				}
				
				
			}
		}
		
		::CloseOpenTransport();

		sSocketsSetup = false;
	}
	
	return(noErr);
}






//	Allocate a socket

OSErr MacSocket_socket(int *outSocketNum,const Boolean inDoThreadSwitching,const long inTimeoutTicks,MacSocket_IdleWaitCallback inIdleWaitCallback,void *inUserRefPtr)
{
//	Gotta roll support back in for threads eventually.....

#pragma unused(inDoThreadSwitching)


OSErr	errCode = noErr;

	
	SetErrorMessageAndBailIfNil(outSocketNum,"MacSocket_socket: Bad parameter, outSocketNum == nil");
	
	*outSocketNum = -1;
	
	
	//	Find an unused socket
	
	for (int i = 0;i < kMaxNumSockets;i++)
	{
		if (sSockets[i].mIsInUse == false)
		{
		OTResult		theOTResult;
		SocketStruct	*theSocketStruct = &(sSockets[i]);
		
			
			InitSocket(theSocketStruct);
			
			theSocketStruct->mIdleWaitCallback = inIdleWaitCallback;
			theSocketStruct->mUserRefPtr = inUserRefPtr;
			
			theSocketStruct->mTimeoutTicks = inTimeoutTicks;
			

			//	Set up OT endpoint
			
			PrepareForAsyncOperation(theSocketStruct,T_OPENCOMPLETE);
			
			theOTResult = ::OTAsyncOpenEndpoint(OTCreateConfiguration(kTCPName),0,nil,OTNonYieldingNotifier,(void *) theSocketStruct);
			
			SetErrorMessageAndLongIntAndBailIfError(theOTResult,"MacSocket_socket: Can't create OT endpoint, OTAsyncOpenEndpoint() = ",theOTResult);
			
			BailIfError(MyBusyWait(theSocketStruct,false,&theOTResult,&(theSocketStruct->mReceivedTOpenComplete)));
																						
			SetErrorMessageAndLongIntAndBailIfError(theOTResult,"MacSocket_socket: Can't create OT endpoint, OTAsyncOpenEndpoint() = ",theOTResult);
			
			
			*outSocketNum = i;
			
			errCode = noErr;
			
			theSocketStruct->mIsInUse = true;
			
			break;
		}
		
		else if (i == kMaxNumSockets - 1)
		{
			SetErrorMessageAndBail("MacSocket_socket: No sockets available");
		}
	}


EXITPOINT:
	
	errno = errCode;
	
	return(errCode);
}




OSErr MacSocket_listen(const int inSocketNum,const int inPortNum)
{
OSErr			errCode = noErr;
SocketStruct	*theSocketStruct = nil;


	if (!SocketIndexIsValid(inSocketNum))
	{
		SetErrorMessageAndBail("MacSocket_listen: Invalid socket number specified");
	}


	theSocketStruct = &(sSockets[inSocketNum]);


OTResult		theOTResult;
	
	
	if (theSocketStruct->mBindRequestedAddrInfo == nil)
	{
		theSocketStruct->mBindRequestedAddrInfo = (TBind *) ::OTAlloc(theSocketStruct->mEndPointRef,T_BIND,T_ADDR,&theOTResult);
																					
		SetErrorMessageAndLongIntAndBailIfError(theOTResult,"MacSocket_listen: Can't allocate OT T_BIND structure, OTAlloc() = ",theOTResult);
		SetErrorMessageAndBailIfNil(theSocketStruct->mBindRequestedAddrInfo,"MacSocket_listen: Can't allocate OT T_BIND structure, OTAlloc() returned nil");
	}
	
	if (theSocketStruct->mAssignedAddrInfo == nil)
	{
		theSocketStruct->mAssignedAddrInfo = (TBind *) ::OTAlloc(theSocketStruct->mEndPointRef,T_BIND,T_ADDR,&theOTResult);
																					
		SetErrorMessageAndLongIntAndBailIfError(theOTResult,"MacSocket_listen: Can't allocate OT T_BIND structure, OTAlloc() = ",theOTResult);
		SetErrorMessageAndBailIfNil(theSocketStruct->mAssignedAddrInfo,"MacSocket_listen: Can't allocate OT T_BIND structure, OTAlloc() returned nil");
	}
	
	if (theSocketStruct->mRemoteAddrInfo == nil)
	{
		theSocketStruct->mRemoteAddrInfo = (TCall *) ::OTAlloc(theSocketStruct->mEndPointRef,T_CALL,T_ADDR,&theOTResult);
																					
		SetErrorMessageAndLongIntAndBailIfError(theOTResult,"MacSocket_listen: Can't allocate OT T_CALL structure, OTAlloc() = ",theOTResult);
		SetErrorMessageAndBailIfNil(theSocketStruct->mRemoteAddrInfo,"MacSocket_listen: Can't allocate OT T_CALL structure, OTAlloc() returned nil");
	}
	

	if (!theSocketStruct->mEndpointIsBound)
	{
	InetInterfaceInfo	theInetInterfaceInfo;
		
		theOTResult = ::OTInetGetInterfaceInfo(&theInetInterfaceInfo,kDefaultInetInterface);
																					
		SetErrorMessageAndLongIntAndBailIfError(theOTResult,"MacSocket_listen: Can't determine OT interface info, OTInetGetInterfaceInfo() = ",theOTResult);


	InetAddress	*theInetAddress = (InetAddress *) theSocketStruct->mBindRequestedAddrInfo->addr.buf;
		
//		theInetAddress->fAddressType = AF_INET;
//		theInetAddress->fPort = inPortNum;
//		theInetAddress->fHost = theInetInterfaceInfo.fAddress;
		
		::OTInitInetAddress(theInetAddress,inPortNum,theInetInterfaceInfo.fAddress);

		theSocketStruct->mBindRequestedAddrInfo->addr.len = sizeof(InetAddress);
		
		theSocketStruct->mBindRequestedAddrInfo->qlen = 1;
		
		
		theOTResult = ::OTSetSynchronous(theSocketStruct->mEndPointRef);
																					
		SetErrorMessageAndLongIntAndBailIfError(theOTResult,"MacSocket_listen: Can't set OT endpoint mode, OTSetSynchronous() = ",theOTResult);
		
		theOTResult = NegotiateIPReuseAddrOption(theSocketStruct->mEndPointRef,true);
																					
		SetErrorMessageAndLongIntAndBailIfError(theOTResult,"MacSocket_listen: Can't set OT IP address reuse flag, NegotiateIPReuseAddrOption() = ",theOTResult);
		
		theOTResult = ::OTSetAsynchronous(theSocketStruct->mEndPointRef);
																					
		SetErrorMessageAndLongIntAndBailIfError(theOTResult,"MacSocket_listen: Can't set OT endpoint mode, OTSetAsynchronous() = ",theOTResult);

		
		PrepareForAsyncOperation(theSocketStruct,T_BINDCOMPLETE);
				
		theOTResult = ::OTBind(theSocketStruct->mEndPointRef,theSocketStruct->mBindRequestedAddrInfo,theSocketStruct->mAssignedAddrInfo);
																					
		SetErrorMessageAndLongIntAndBailIfError(theOTResult,"MacSocket_listen: Can't bind OT endpoint, OTBind() = ",theOTResult);
		
		BailIfError(MyBusyWait(theSocketStruct,false,&theOTResult,&(theSocketStruct->mReceivedTBindComplete)));
																					
		SetErrorMessageAndLongIntAndBailIfError(theOTResult,"MacSocket_listen: Can't bind OT endpoint, OTBind() = ",theOTResult);
		
		
		theSocketStruct->mEndpointIsBound = true;
	}


	PrepareForAsyncOperation(theSocketStruct,T_LISTEN);

	theOTResult = ::OTListen(theSocketStruct->mEndPointRef,theSocketStruct->mRemoteAddrInfo);
	
	if (theOTResult == noErr)
	{
		PrepareForAsyncOperation(theSocketStruct,T_PASSCON);
		
		theOTResult = ::OTAccept(theSocketStruct->mEndPointRef,theSocketStruct->mEndPointRef,theSocketStruct->mRemoteAddrInfo);
		
		SetErrorMessageAndLongIntAndBailIfError(theOTResult,"MacSocket_listen: Can't begin OT accept, OTAccept() = ",theOTResult);
		
		BailIfError(MyBusyWait(theSocketStruct,false,&theOTResult,&(theSocketStruct->mReceivedTPassCon)));
																					
		SetErrorMessageAndLongIntAndBailIfError(theOTResult,"MacSocket_listen: Can't accept OT connection, OTAccept() = ",theOTResult);
	}
	
	else if (theOTResult == kOTNoDataErr)
	{
		theOTResult = noErr;
	}
	
	else
	{
		SetErrorMessageAndLongIntAndBail("MacSocket_listen: Can't begin OT listen, OTListen() = ",theOTResult);
	}


	errCode = noErr;


EXITPOINT:
	
	if (theSocketStruct != nil)
	{
		theSocketStruct->mLastError = noErr;
		
		CopyCStrToCStr("",theSocketStruct->mErrMessage,sizeof(theSocketStruct->mErrMessage));

		if (errCode != noErr)
		{
			theSocketStruct->mLastError = errCode;
			
			CopyCStrToCStr(GetErrorMessage(),theSocketStruct->mErrMessage,sizeof(theSocketStruct->mErrMessage));
		}
	}
	
	errno = errCode;
	
	return(errCode);
}




OSErr MacSocket_connect(const int inSocketNum,char *inTargetAddressAndPort)
{
OSErr			errCode = noErr;
SocketStruct	*theSocketStruct = nil;


	if (!SocketIndexIsValid(inSocketNum))
	{
		SetErrorMessageAndBail("MacSocket_connect: Invalid socket number specified");
	}

	theSocketStruct = &(sSockets[inSocketNum]);

	if (theSocketStruct->mEndpointIsBound)
	{
		SetErrorMessageAndBail("MacSocket_connect: Socket previously bound");
	}

	
OTResult		theOTResult;

	theSocketStruct->mBindRequestedAddrInfo = (TBind *) ::OTAlloc(theSocketStruct->mEndPointRef,T_BIND,T_ADDR,&theOTResult);
																				
	SetErrorMessageAndLongIntAndBailIfError(theOTResult,"MacSocket_connect: Can't allocate OT T_BIND structure, OTAlloc() = ",theOTResult);
	SetErrorMessageAndBailIfNil(theSocketStruct->mBindRequestedAddrInfo,"MacSocket_connect: Can't allocate OT T_BIND structure, OTAlloc() returned nil");
	

	theSocketStruct->mAssignedAddrInfo = (TBind *) ::OTAlloc(theSocketStruct->mEndPointRef,T_BIND,T_ADDR,&theOTResult);
																				
	SetErrorMessageAndLongIntAndBailIfError(theOTResult,"MacSocket_connect: Can't allocate OT T_BIND structure, OTAlloc() = ",theOTResult);
	SetErrorMessageAndBailIfNil(theSocketStruct->mAssignedAddrInfo,"MacSocket_connect: Can't allocate OT T_BIND structure, OTAlloc() returned nil");


	theSocketStruct->mRemoteAddrInfo = (TCall *) ::OTAlloc(theSocketStruct->mEndPointRef,T_CALL,T_ADDR,&theOTResult);
																				
	SetErrorMessageAndLongIntAndBailIfError(theOTResult,"MacSocket_connect: Can't allocate OT T_CALL structure, OTAlloc() = ",theOTResult);
	SetErrorMessageAndBailIfNil(theSocketStruct->mRemoteAddrInfo,"MacSocket_connect: Can't allocate OT T_CALL structure, OTAlloc() returned nil");

	
	PrepareForAsyncOperation(theSocketStruct,T_BINDCOMPLETE);

	theOTResult = ::OTBind(theSocketStruct->mEndPointRef,nil,theSocketStruct->mAssignedAddrInfo);
																				
	SetErrorMessageAndLongIntAndBailIfError(theOTResult,"MacSocket_connect: Can't bind OT endpoint, OTBind() = ",theOTResult);
	
	BailIfError(MyBusyWait(theSocketStruct,false,&theOTResult,&(theSocketStruct->mReceivedTBindComplete)));
																				
	SetErrorMessageAndLongIntAndBailIfError(theOTResult,"MacSocket_connect: Can't bind OT endpoint, OTBind() = ",theOTResult);
	
	theSocketStruct->mEndpointIsBound = true;
	

TCall		sndCall;
DNSAddress 	hostDNSAddress;
	
	//	Set up target address
	
	sndCall.addr.buf = (UInt8 *) &hostDNSAddress;
	sndCall.addr.len = ::OTInitDNSAddress(&hostDNSAddress,inTargetAddressAndPort);
	sndCall.opt.buf = nil;
	sndCall.opt.len = 0;
	sndCall.udata.buf = nil;
	sndCall.udata.len = 0;
	sndCall.sequence = 0;
		
	//	Connect!
	
	PrepareForAsyncOperation(theSocketStruct,T_CONNECT);

	theOTResult = ::OTConnect(theSocketStruct->mEndPointRef,&sndCall,nil);
	
	if (theOTResult == kOTNoDataErr)
	{
		theOTResult = noErr;
	}
												
	SetErrorMessageAndLongIntAndBailIfError(theOTResult,"MacSocket_connect: Can't connect OT endpoint, OTConnect() = ",theOTResult);
	
	BailIfError(MyBusyWait(theSocketStruct,false,&theOTResult,&(theSocketStruct->mReceivedTConnect)));
	
	if (theOTResult == kMacSocket_TimeoutErr)
	{
		SetErrorMessageAndBail("MacSocket_connect: Can't connect OT endpoint, OTConnect() = kMacSocket_TimeoutErr");
	}
	
	else
	{
		SetErrorMessageAndLongIntAndBailIfError(theOTResult,"MacSocket_connect: Can't connect OT endpoint, OTConnect() = ",theOTResult);
	}

	theOTResult = ::OTRcvConnect(theSocketStruct->mEndPointRef,nil);
												
	SetErrorMessageAndLongIntAndBailIfError(theOTResult,"MacSocket_connect: Can't complete connect on OT endpoint, OTRcvConnect() = ",theOTResult);


	errCode = noErr;


#ifdef MACSOCKET_DEBUG
	printf("MacSocket_connect: connect completed\n");
#endif

EXITPOINT:
	
	if (theSocketStruct != nil)
	{
		theSocketStruct->mLastError = noErr;
		
		CopyCStrToCStr("",theSocketStruct->mErrMessage,sizeof(theSocketStruct->mErrMessage));

		if (errCode != noErr)
		{
			theSocketStruct->mLastError = errCode;
			
			CopyCStrToCStr(GetErrorMessage(),theSocketStruct->mErrMessage,sizeof(theSocketStruct->mErrMessage));
		}
	}
	
	errno = errCode;
	
	return(errCode);
}




//	Close a connection

OSErr MacSocket_close(const int inSocketNum)
{
OSErr			errCode = noErr;
SocketStruct	*theSocketStruct = nil;


	if (!SocketIndexIsValid(inSocketNum))
	{
		SetErrorMessageAndBail("MacSocket_close: Invalid socket number specified");
	}


	theSocketStruct = &(sSockets[inSocketNum]);
	
	if (theSocketStruct->mEndPointRef != kOTInvalidEndpointRef)
	{
	OTResult		theOTResult = noErr;
	
		//	Try to play nice
		
		if (theSocketStruct->mReceivedTOrdRel)
		{
			//	Already did an OTRcvOrderlyDisconnect() in the notifier
		
			if (theSocketStruct->mLocalEndIsConnected)
			{
				theOTResult = ::OTSndOrderlyDisconnect(theSocketStruct->mEndPointRef);
				
				theSocketStruct->mLocalEndIsConnected = false;
			}
		}
		
		else if (theSocketStruct->mLocalEndIsConnected)
		{
			theOTResult = ::OTSndOrderlyDisconnect(theSocketStruct->mEndPointRef);
			
			theSocketStruct->mLocalEndIsConnected = false;
			
			//	Wait for other end to hang up too!
			
//			PrepareForAsyncOperation(theSocketStruct,T_ORDREL);
//
//			errCode = MyBusyWait(theSocketStruct,false,&theOTResult,&(theSocketStruct->mReceivedTOrdRel));
		}
		
		
		if (theOTResult != noErr)
		{
			::OTCloseProvider(theSocketStruct->mEndPointRef);
		}
		
		else
		{
			theOTResult = ::OTCloseProvider(theSocketStruct->mEndPointRef);
		}

		theSocketStruct->mEndPointRef = kOTInvalidEndpointRef;
		
		errCode = theOTResult;
	}


	theSocketStruct->mIsInUse = false;

	
EXITPOINT:
	
	if (theSocketStruct != nil)
	{
		theSocketStruct->mLastError = noErr;
		
		CopyCStrToCStr("",theSocketStruct->mErrMessage,sizeof(theSocketStruct->mErrMessage));

		if (errCode != noErr)
		{
			theSocketStruct->mLastError = errCode;
			
			CopyCStrToCStr(GetErrorMessage(),theSocketStruct->mErrMessage,sizeof(theSocketStruct->mErrMessage));
		}
	}

	errno = errCode;
		
	return(errCode);
}




//	Receive some bytes

int MacSocket_recv(const int inSocketNum,void *outBuff,int outBuffLength,const Boolean inBlock)
{
OSErr			errCode = noErr;
int				totalBytesRead = 0;
SocketStruct	*theSocketStruct = nil;

	
	SetErrorMessageAndBailIfNil(outBuff,"MacSocket_recv: Bad parameter, outBuff = nil");
	
	if (outBuffLength <= 0)
	{
		SetErrorMessageAndBail("MacSocket_recv: Bad parameter, outBuffLength <= 0");
	}
	
	if (!SocketIndexIsValid(inSocketNum))
	{
		SetErrorMessageAndBail("MacSocket_recv: Invalid socket number specified");
	}

	theSocketStruct = &(sSockets[inSocketNum]);

	if (!theSocketStruct->mLocalEndIsConnected)
	{
		SetErrorMessageAndBail("MacSocket_recv: Socket not connected");
	}

	if (theSocketStruct->mReceivedTOrdRel)
	{
		totalBytesRead = 0;
		
		goto EXITPOINT;
	}

	
	PrepareForAsyncOperation(theSocketStruct,0);
	
	for (;;)
	{
	int			bytesRead;
	OTResult	theOTResult;
	
	
		theOTResult = ::OTRcv(theSocketStruct->mEndPointRef,(void *) ((unsigned long) outBuff + (unsigned long) totalBytesRead),outBuffLength - totalBytesRead,nil);
		
		if (theOTResult >= 0)
		{
			bytesRead = theOTResult;
			
#ifdef MACSOCKET_DEBUG
	printf("MacSocket_recv: read %d bytes in part\n",bytesRead);
#endif
		}
		
		else if (theOTResult == kOTNoDataErr)
		{
			bytesRead = 0;
		}
		
		else
		{
			SetErrorMessageAndLongIntAndBail("MacSocket_recv: Can't receive OT data, OTRcv() = ",theOTResult);
		}
		
		
		totalBytesRead += bytesRead;
		
		
		if (totalBytesRead <= 0)
		{
			if (theSocketStruct->mReceivedTOrdRel)
			{
				break;
			}
			
			//	This seems pretty stupid to me now.  Maybe I'll delete this blocking garbage.
			
			if (inBlock)
			{
				if (TimeoutElapsed(theSocketStruct))
				{
					SetErrorCodeAndMessageAndBail(kMacSocket_TimeoutErr,"MacSocket_recv: Receive operation timed-out");
				}
				
				if (theSocketStruct->mIdleWaitCallback != nil)
				{
					theOTResult = (*(theSocketStruct->mIdleWaitCallback))(theSocketStruct->mUserRefPtr);
					
					SetErrorMessageAndBailIfError(theOTResult,"MacSocket_recv: User cancelled operation");
				}
				
				continue;
			}
		}
		
		
		break;
	}
	
	errCode = noErr;


#ifdef MACSOCKET_DEBUG
	printf("MacSocket_recv: read %d bytes in total\n",totalBytesRead);
#endif
	
	
EXITPOINT:
	
	if (theSocketStruct != nil)
	{
		theSocketStruct->mLastError = noErr;
		
		CopyCStrToCStr("",theSocketStruct->mErrMessage,sizeof(theSocketStruct->mErrMessage));

		if (errCode != noErr)
		{
			theSocketStruct->mLastError = errCode;
			
			CopyCStrToCStr(GetErrorMessage(),theSocketStruct->mErrMessage,sizeof(theSocketStruct->mErrMessage));
		}
	}

	errno = errCode;
	
	return(totalBytesRead);
}



//	Send some bytes

int MacSocket_send(const int inSocketNum,const void *inBuff,int inBuffLength)
{
OSErr			errCode = noErr;
int				bytesSent = 0;
SocketStruct	*theSocketStruct = nil;


	SetErrorMessageAndBailIfNil(inBuff,"MacSocket_send: Bad parameter, inBuff = nil");
	
	if (inBuffLength <= 0)
	{
		SetErrorMessageAndBail("MacSocket_send: Bad parameter, inBuffLength <= 0");
	}

	if (!SocketIndexIsValid(inSocketNum))
	{
		SetErrorMessageAndBail("MacSocket_send: Invalid socket number specified");
	}
	

	theSocketStruct = &(sSockets[inSocketNum]);
	
	if (!theSocketStruct->mLocalEndIsConnected)
	{
		SetErrorMessageAndBail("MacSocket_send: Socket not connected");
	}


OTResult		theOTResult;
	

	PrepareForAsyncOperation(theSocketStruct,0);

	while (bytesSent < inBuffLength)
	{
		if (theSocketStruct->mIdleWaitCallback != nil)
		{
			theOTResult = (*(theSocketStruct->mIdleWaitCallback))(theSocketStruct->mUserRefPtr);
			
			SetErrorMessageAndBailIfError(theOTResult,"MacSocket_send: User cancelled");
		}


		theOTResult = ::OTSnd(theSocketStruct->mEndPointRef,(void *) ((unsigned long) inBuff + bytesSent),inBuffLength - bytesSent,0);
		
		if (theOTResult >= 0)
		{
			bytesSent += theOTResult;
			
			theOTResult = noErr;
			
			//	Reset timer....
			
			PrepareForAsyncOperation(theSocketStruct,0);
		}
		
		if (theOTResult == kOTFlowErr)
		{
			if (TimeoutElapsed(theSocketStruct))
			{
				SetErrorCodeAndMessageAndBail(kMacSocket_TimeoutErr,"MacSocket_send: Send timed-out")
			}

			theOTResult = noErr;
		}
													
		SetErrorMessageAndLongIntAndBailIfError(theOTResult,"MacSocket_send: Can't send OT data, OTSnd() = ",theOTResult);
	}

	
	errCode = noErr;

#ifdef MACSOCKET_DEBUG
	printf("MacSocket_send: sent %d bytes\n",bytesSent);
#endif
	
	
EXITPOINT:
	
	if (theSocketStruct != nil)
	{
		theSocketStruct->mLastError = noErr;
		
		CopyCStrToCStr("",theSocketStruct->mErrMessage,sizeof(theSocketStruct->mErrMessage));

		if (errCode != noErr)
		{
			theSocketStruct->mLastError = errCode;
			
			CopyCStrToCStr(GetErrorMessage(),theSocketStruct->mErrMessage,sizeof(theSocketStruct->mErrMessage));
		}
	}
	
	if (errCode != noErr)
	{
		::SysBeep(1);
	}
	
	errno = errCode;
	
	return(bytesSent);
}





static OSStatus NegotiateIPReuseAddrOption(EndpointRef inEndpoint,const Boolean inEnableReuseIP)
{
OSStatus	errCode;
UInt8		buf[kOTFourByteOptionSize];
TOption*	theOTOption;
TOptMgmt	theOTRequest;
TOptMgmt	theOTResult;
	

	if (!OTIsSynchronous(inEndpoint))
	{
		SetErrorMessageAndBail("NegotiateIPReuseAddrOption: Open Transport endpoint is not synchronous");
	}
	
	theOTRequest.opt.buf = buf;
	theOTRequest.opt.len = sizeof(buf);
	theOTRequest.flags = T_NEGOTIATE;

	theOTResult.opt.buf = buf;
	theOTResult.opt.maxlen = kOTFourByteOptionSize;


	theOTOption = (TOption *) buf;
	
	theOTOption->level = INET_IP;
	theOTOption->name = IP_REUSEADDR;
	theOTOption->len = kOTFourByteOptionSize;
	theOTOption->status = 0;
	*((UInt32 *) (theOTOption->value)) = inEnableReuseIP;

	errCode = ::OTOptionManagement(inEndpoint,&theOTRequest,&theOTResult);
	
	if (errCode == kOTNoError)
	{
		if (theOTOption->status != T_SUCCESS)
		{
			errCode = theOTOption->status;
		}
		
		else
		{
			errCode = kOTNoError;
		}
	}
				

EXITPOINT:
	
	errno = errCode;
	
	return(errCode);
}





//	Some rough notes....



//	OTAckSends(ep);
//	OTAckSends(ep) // enable AckSend option
//	......
//	buf = OTAllocMem( nbytes); // Allocate nbytes of memory from OT
//	OTSnd(ep, buf, nbytes, 0); // send a packet
//	......
//	NotifyProc( .... void* theParam) // Notifier Proc
//	case T_MEMORYRELEASED: // process event
//	OTFreeMem( theParam); // free up memory
//	break;



/*
struct InetInterfaceInfo
{
	InetHost		fAddress;
	InetHost		fNetmask;
	InetHost		fBroadcastAddr;
	InetHost		fDefaultGatewayAddr;
	InetHost		fDNSAddr;
	UInt16			fVersion;
	UInt16			fHWAddrLen;
	UInt8*			fHWAddr;
	UInt32			fIfMTU;
	UInt8*			fReservedPtrs[2];
	InetDomainName	fDomainName;
	UInt32			fIPSecondaryCount;
	UInt8			fReserved[252];			
};
typedef struct InetInterfaceInfo InetInterfaceInfo;



((InetAddress *) addr.buf)->fHost

struct TBind
{
	TNetbuf	addr;
	OTQLen	qlen;
};

typedef struct TBind	TBind;

struct TNetbuf
{
	size_t	maxlen;
	size_t	len;
	UInt8*	buf;
};

typedef struct TNetbuf	TNetbuf;

	
	struct InetAddress
{
		OTAddressType	fAddressType;	// always AF_INET
		InetPort		fPort;			// Port number 
		InetHost		fHost;			// Host address in net byte order
		UInt8			fUnused[8];		// Traditional unused bytes
};
typedef struct InetAddress InetAddress;
*/



/*
static pascal void Notifier(void* context, OTEventCode event, OTResult result, void* cookie)
{
EPInfo* epi = (EPInfo*) context;

	switch (event)
	{
		case T_LISTEN:
		{
			DoListenAccept();
			return;
		}
		
		case T_ACCEPTCOMPLETE:
		{
			if (result != kOTNoError)
				DBAlert1("Notifier: T_ACCEPTCOMPLETE - result %d",result);
			return;
		}
		
		case T_PASSCON:
		{
			if (result != kOTNoError)
			{
				DBAlert1("Notifier: T_PASSCON result %d", result);
				return;
			}

			OTAtomicAdd32(1, &gCntrConnections);
			OTAtomicAdd32(1, &gCntrTotalConnections);
			OTAtomicAdd32(1, &gCntrIntervalConnects);
			
			if ( OTAtomicSetBit(&epi->stateFlags, kPassconBit) != 0 )
			{
				ReadData(epi);
			}
			
			return;
		}
		
		case T_DATA:
		{
			if ( OTAtomicSetBit(&epi->stateFlags, kPassconBit) != 0 )
			{
				ReadData(epi);
			}
			
			return;
		}
		
		case T_GODATA:
		{
			SendData(epi);
			return;
		}
		
		case T_DISCONNECT:
		{
			DoRcvDisconnect(epi);
			return;
		}
		
		case T_DISCONNECTCOMPLETE:
		{
			if (result != kOTNoError)
				DBAlert1("Notifier: T_DISCONNECT_COMPLETE result %d",result);
				
			return;
		}
		
		case T_MEMORYRELEASED:
		{
			OTAtomicAdd32(-1, &epi->outstandingSends);
			return;
		}
		
		default:
		{
			DBAlert1("Notifier: unknown event <%x>", event);
			return;
		}
	}
}
*/
