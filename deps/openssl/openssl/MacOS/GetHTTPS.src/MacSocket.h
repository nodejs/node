#pragma once


#ifdef __cplusplus
extern "C" {
#endif



enum
{
	kMacSocket_TimeoutErr = -2
};


//	Since MacSocket does busy waiting, I do a callback while waiting

typedef OSErr (*MacSocket_IdleWaitCallback)(void *);


//	Call this before anything else!

OSErr MacSocket_Startup(void);


//	Call this to cleanup before quitting

OSErr MacSocket_Shutdown(void);


//	Call this to allocate a "socket" (reference number is returned in outSocketNum)
//	Note that inDoThreadSwitching is pretty much irrelevant right now, since I ignore it
//	The inTimeoutTicks parameter is applied during reads/writes of data
//	The inIdleWaitCallback parameter specifies a callback which is called during busy-waiting periods
//	The inUserRefPtr parameter is passed back to the idle-wait callback

OSErr MacSocket_socket(int *outSocketNum,const Boolean inDoThreadSwitching,const long inTimeoutTicks,MacSocket_IdleWaitCallback inIdleWaitCallback,void *inUserRefPtr);


//	Call this to connect to an IP/DNS address
//	Note that inTargetAddressAndPort is in "IP:port" format-- e.g. 10.1.1.1:123

OSErr MacSocket_connect(const int inSocketNum,char *inTargetAddressAndPort);


//	Call this to listen on a port
//	Since this a low-performance implementation, I allow a maximum of 1 (one!) incoming request when I listen

OSErr MacSocket_listen(const int inSocketNum,const int inPortNum);


//	Call this to close a socket

OSErr MacSocket_close(const int inSocketNum);


//	Call this to receive data on a socket
//	Most parameters' purpose are obvious-- except maybe "inBlock" which controls whether I wait for data or return immediately

int MacSocket_recv(const int inSocketNum,void *outBuff,int outBuffLength,const Boolean inBlock);


//	Call this to send data on a socket

int MacSocket_send(const int inSocketNum,const void *inBuff,int inBuffLength);


//	If zero bytes were read in a call to MacSocket_recv(), it may be that the remote end has done a half-close
//	This function will let you check whether that's true or not

Boolean MacSocket_RemoteEndIsClosing(const int inSocketNum);


//	Call this to see if the listen has completed after a call to MacSocket_listen()

Boolean MacSocket_ListenCompleted(const int inSocketNum);


//	These really aren't very useful anymore

Boolean MacSocket_LocalEndIsOpen(const int inSocketNum);
Boolean MacSocket_RemoteEndIsOpen(const int inSocketNum);


//	You may wish to change the userRefPtr for a socket callback-- use this to do it

void MacSocket_SetUserRefPtr(const int inSocketNum,void *inNewRefPtr);


//	Call these to get the socket's IP:port descriptor

void MacSocket_GetLocalIPAndPort(const int inSocketNum,char *outIPAndPort,const int inIPAndPortLength);
void MacSocket_GetRemoteIPAndPort(const int inSocketNum,char *outIPAndPort,const int inIPAndPortLength);


//	Call this to get error info from a socket

void MacSocket_GetSocketErrorInfo(const int inSocketNum,int *outSocketErrCode,char *outSocketErrString,const int inSocketErrStringMaxLength);


#ifdef __cplusplus
}
#endif
