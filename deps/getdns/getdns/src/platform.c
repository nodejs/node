/**
 *
 * \file platform.c
 * @brief general functions with platform-dependent implementations
 *
 */

/*
 * Copyright (c) 2017, NLnet Labs, Sinodun
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * * Neither the names of the copyright holders nor the
 *   names of its contributors may be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Verisign, Inc. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform.h"

#include <stdio.h>

#ifdef USE_WINSOCK

void _getdns_perror(const char *str)
{
        char msg[256];
        int errid = WSAGetLastError();

        *msg = '\0';
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |
                          FORMAT_MESSAGE_IGNORE_INSERTS,
                      NULL,
                      errid,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                      msg,
                      sizeof(msg),
                      NULL);
        if (*msg == '\0')
                sprintf(msg, "Unknown error: %d", errid);
        if (str && *str != '\0')
                fprintf(stderr, "%s: ", str);
        fputs(msg, stderr);
}

const char *_getdns_strerror(DWORD errnum)
{
	static char unknown[32];

	switch(errnum) {
	case WSA_INVALID_HANDLE: return "Specified event object handle is invalid.";
	case WSA_NOT_ENOUGH_MEMORY: return "Insufficient memory available.";
	case WSA_INVALID_PARAMETER: return "One or more parameters are invalid.";
	case WSA_OPERATION_ABORTED: return "Overlapped operation aborted.";
	case WSA_IO_INCOMPLETE: return "Overlapped I/O event object not in signaled state.";
	case WSA_IO_PENDING: return "Overlapped operations will complete later.";
	case WSAEINTR: return "Interrupted function call.";
	case WSAEBADF: return "File handle is not valid.";
	case WSAEACCES: return "Permission denied.";
	case WSAEFAULT: return "Bad address.";
	case WSAEINVAL: return "Invalid argument.";
	case WSAEMFILE: return "Too many open files.";
	case WSAEWOULDBLOCK: return "Resource temporarily unavailable.";
	case WSAEINPROGRESS: return "Operation now in progress.";
	case WSAEALREADY: return "Operation already in progress.";
	case WSAENOTSOCK: return "Socket operation on nonsocket.";
	case WSAEDESTADDRREQ: return "Destination address required.";
	case WSAEMSGSIZE: return "Message too long.";
	case WSAEPROTOTYPE: return "Protocol wrong type for socket.";
	case WSAENOPROTOOPT: return "Bad protocol option.";
	case WSAEPROTONOSUPPORT: return "Protocol not supported.";
	case WSAESOCKTNOSUPPORT: return "Socket type not supported.";
	case WSAEOPNOTSUPP: return "Operation not supported.";
	case WSAEPFNOSUPPORT: return "Protocol family not supported.";
	case WSAEAFNOSUPPORT: return "Address family not supported by protocol family.";
	case WSAEADDRINUSE: return "Address already in use.";
	case WSAEADDRNOTAVAIL: return "Cannot assign requested address.";
	case WSAENETDOWN: return "Network is down.";
	case WSAENETUNREACH: return "Network is unreachable.";
	case WSAENETRESET: return "Network dropped connection on reset.";
	case WSAECONNABORTED: return "Software caused connection abort.";
	case WSAECONNRESET: return "Connection reset by peer.";
	case WSAENOBUFS: return "No buffer space available.";
	case WSAEISCONN: return "Socket is already connected.";
	case WSAENOTCONN: return "Socket is not connected.";
	case WSAESHUTDOWN: return "Cannot send after socket shutdown.";
	case WSAETOOMANYREFS: return "Too many references.";
	case WSAETIMEDOUT: return "Connection timed out.";
	case WSAECONNREFUSED: return "Connection refused.";
	case WSAELOOP: return "Cannot translate name.";
	case WSAENAMETOOLONG: return "Name too long.";
	case WSAEHOSTDOWN: return "Host is down.";
	case WSAEHOSTUNREACH: return "No route to host.";
	case WSAENOTEMPTY: return "Directory not empty.";
	case WSAEPROCLIM: return "Too many processes.";
	case WSAEUSERS: return "User quota exceeded.";
	case WSAEDQUOT: return "Disk quota exceeded.";
	case WSAESTALE: return "Stale file handle reference.";
	case WSAEREMOTE: return "Item is remote.";
	case WSASYSNOTREADY: return "Network subsystem is unavailable.";
	case WSAVERNOTSUPPORTED: return "Winsock.dll version out of range.";
	case WSANOTINITIALISED: return "Successful WSAStartup not yet performed.";
	case WSAEDISCON: return "Graceful shutdown in progress.";
	case WSAENOMORE: return "No more results.";
	case WSAECANCELLED: return "Call has been canceled.";
	case WSAEINVALIDPROCTABLE: return "Procedure call table is invalid.";
	case WSAEINVALIDPROVIDER: return "Service provider is invalid.";
	case WSAEPROVIDERFAILEDINIT: return "Service provider failed to initialize.";
	case WSASYSCALLFAILURE: return "System call failure.";
	case WSASERVICE_NOT_FOUND: return "Service not found.";
	case WSATYPE_NOT_FOUND: return "Class type not found.";
	case WSA_E_NO_MORE: return "No more results.";
	case WSA_E_CANCELLED: return "Call was canceled.";
	case WSAEREFUSED: return "Database query was refused.";
	case WSAHOST_NOT_FOUND: return "Host not found.";
	case WSATRY_AGAIN: return "Nonauthoritative host not found.";
	case WSANO_RECOVERY: return "This is a nonrecoverable error.";
	case WSANO_DATA: return "Valid name, no data record of requested type.";
	case WSA_QOS_RECEIVERS: return "QOS receivers.";
	case WSA_QOS_SENDERS: return "QOS senders.";
	case WSA_QOS_NO_SENDERS: return "No QOS senders.";
	case WSA_QOS_NO_RECEIVERS: return "QOS no receivers.";
	case WSA_QOS_REQUEST_CONFIRMED: return "QOS request confirmed.";
	case WSA_QOS_ADMISSION_FAILURE: return "QOS admission error.";
	case WSA_QOS_POLICY_FAILURE: return "QOS policy failure.";
	case WSA_QOS_BAD_STYLE: return "QOS bad style.";
	case WSA_QOS_BAD_OBJECT: return "QOS bad object.";
	case WSA_QOS_TRAFFIC_CTRL_ERROR: return "QOS traffic control error.";
	case WSA_QOS_GENERIC_ERROR: return "QOS generic error.";
	case WSA_QOS_ESERVICETYPE: return "QOS service type error.";
	case WSA_QOS_EFLOWSPEC: return "QOS flowspec error.";
	case WSA_QOS_EPROVSPECBUF: return "Invalid QOS provider buffer.";
	case WSA_QOS_EFILTERSTYLE: return "Invalid QOS filter style.";
	case WSA_QOS_EFILTERTYPE: return "Invalid QOS filter type.";
	case WSA_QOS_EFILTERCOUNT: return "Incorrect QOS filter count.";
	case WSA_QOS_EOBJLENGTH: return "Invalid QOS object length.";
	case WSA_QOS_EFLOWCOUNT: return "Incorrect QOS flow count.";
	/*case WSA_QOS_EUNKOWNPSOBJ: return "Unrecognized QOS object.";*/
	case WSA_QOS_EPOLICYOBJ: return "Invalid QOS policy object.";
	case WSA_QOS_EFLOWDESC: return "Invalid QOS flow descriptor.";
	case WSA_QOS_EPSFLOWSPEC: return "Invalid QOS provider-specific flowspec.";
	case WSA_QOS_EPSFILTERSPEC: return "Invalid QOS provider-specific filterspec.";
	case WSA_QOS_ESDMODEOBJ: return "Invalid QOS shape discard mode object.";
	case WSA_QOS_ESHAPERATEOBJ: return "Invalid QOS shaping rate object.";
	case WSA_QOS_RESERVED_PETYPE: return "Reserved policy QOS element type.";
	default:
		snprintf(unknown, sizeof(unknown),
			"unknown WSA error code %d", (int)errnum);
		return unknown;
	}
}

#else

void _getdns_perror(const char *str)
{
        perror(str);
}

const char *_getdns_strerror(int errnum)
{
	return strerror(errnum);
}
#endif
