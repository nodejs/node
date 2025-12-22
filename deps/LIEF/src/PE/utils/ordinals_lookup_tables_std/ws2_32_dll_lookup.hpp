/* Copyright 2017 - 2025 R. Thomas
 * Copyright 2017 - 2025 Quarkslab
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef LIEF_PE_WS2_32_DLL_LOOKUP_STD_H
#define LIEF_PE_WS2_32_DLL_LOOKUP_STD_H

#include <cinttypes>

namespace LIEF {
namespace PE {
namespace imphashstd {

// From pefile: https://github.com/erocarrera/pefile/blob/09264be6f731bf8578aee8638cc4046154e03abf/ordlookup/ws2_32.py
inline const char* ws2_32_dll_lookup(uint32_t i) {
  switch(i) {
  case 0x0001: return "accept";
  case 0x0002: return "bind";
  case 0x0003: return "closesocket";
  case 0x0004: return "connect";
  case 0x0005: return "getpeername";
  case 0x0006: return "getsockname";
  case 0x0007: return "getsockopt";
  case 0x0008: return "htonl";
  case 0x0009: return "htons";
  case 0x000a: return "ioctlsocket";
  case 0x000b: return "inet_addr";
  case 0x000c: return "inet_ntoa";
  case 0x000d: return "listen";
  case 0x000e: return "ntohl";
  case 0x000f: return "ntohs";
  case 0x0010: return "recv";
  case 0x0011: return "recvfrom";
  case 0x0012: return "select";
  case 0x0013: return "send";
  case 0x0014: return "sendto";
  case 0x0015: return "setsockopt";
  case 0x0016: return "shutdown";
  case 0x0017: return "socket";
  case 0x0018: return "GetAddrInfoW";
  case 0x0019: return "GetNameInfoW";
  case 0x001a: return "WSApSetPostRoutine";
  case 0x001b: return "FreeAddrInfoW";
  case 0x001c: return "WPUCompleteOverlappedRequest";
  case 0x001d: return "WSAAccept";
  case 0x001e: return "WSAAddressToStringA";
  case 0x001f: return "WSAAddressToStringW";
  case 0x0020: return "WSACloseEvent";
  case 0x0021: return "WSAConnect";
  case 0x0022: return "WSACreateEvent";
  case 0x0023: return "WSADuplicateSocketA";
  case 0x0024: return "WSADuplicateSocketW";
  case 0x0025: return "WSAEnumNameSpaceProvidersA";
  case 0x0026: return "WSAEnumNameSpaceProvidersW";
  case 0x0027: return "WSAEnumNetworkEvents";
  case 0x0028: return "WSAEnumProtocolsA";
  case 0x0029: return "WSAEnumProtocolsW";
  case 0x002a: return "WSAEventSelect";
  case 0x002b: return "WSAGetOverlappedResult";
  case 0x002c: return "WSAGetQOSByName";
  case 0x002d: return "WSAGetServiceClassInfoA";
  case 0x002e: return "WSAGetServiceClassInfoW";
  case 0x002f: return "WSAGetServiceClassNameByClassIdA";
  case 0x0030: return "WSAGetServiceClassNameByClassIdW";
  case 0x0031: return "WSAHtonl";
  case 0x0032: return "WSAHtons";
  case 0x0033: return "gethostbyaddr";
  case 0x0034: return "gethostbyname";
  case 0x0035: return "getprotobyname";
  case 0x0036: return "getprotobynumber";
  case 0x0037: return "getservbyname";
  case 0x0038: return "getservbyport";
  case 0x0039: return "gethostname";
  case 0x003a: return "WSAInstallServiceClassA";
  case 0x003b: return "WSAInstallServiceClassW";
  case 0x003c: return "WSAIoctl";
  case 0x003d: return "WSAJoinLeaf";
  case 0x003e: return "WSALookupServiceBeginA";
  case 0x003f: return "WSALookupServiceBeginW";
  case 0x0040: return "WSALookupServiceEnd";
  case 0x0041: return "WSALookupServiceNextA";
  case 0x0042: return "WSALookupServiceNextW";
  case 0x0043: return "WSANSPIoctl";
  case 0x0044: return "WSANtohl";
  case 0x0045: return "WSANtohs";
  case 0x0046: return "WSAProviderConfigChange";
  case 0x0047: return "WSARecv";
  case 0x0048: return "WSARecvDisconnect";
  case 0x0049: return "WSARecvFrom";
  case 0x004a: return "WSARemoveServiceClass";
  case 0x004b: return "WSAResetEvent";
  case 0x004c: return "WSASend";
  case 0x004d: return "WSASendDisconnect";
  case 0x004e: return "WSASendTo";
  case 0x004f: return "WSASetEvent";
  case 0x0050: return "WSASetServiceA";
  case 0x0051: return "WSASetServiceW";
  case 0x0052: return "WSASocketA";
  case 0x0053: return "WSASocketW";
  case 0x0054: return "WSAStringToAddressA";
  case 0x0055: return "WSAStringToAddressW";
  case 0x0056: return "WSAWaitForMultipleEvents";
  case 0x0057: return "WSCDeinstallProvider";
  case 0x0058: return "WSCEnableNSProvider";
  case 0x0059: return "WSCEnumProtocols";
  case 0x005a: return "WSCGetProviderPath";
  case 0x005b: return "WSCInstallNameSpace";
  case 0x005c: return "WSCInstallProvider";
  case 0x005d: return "WSCUnInstallNameSpace";
  case 0x005e: return "WSCUpdateProvider";
  case 0x005f: return "WSCWriteNameSpaceOrder";
  case 0x0060: return "WSCWriteProviderOrder";
  case 0x0061: return "freeaddrinfo";
  case 0x0062: return "getaddrinfo";
  case 0x0063: return "getnameinfo";
  case 0x0065: return "WSAAsyncSelect";
  case 0x0066: return "WSAAsyncGetHostByAddr";
  case 0x0067: return "WSAAsyncGetHostByName";
  case 0x0068: return "WSAAsyncGetProtoByNumber";
  case 0x0069: return "WSAAsyncGetProtoByName";
  case 0x006a: return "WSAAsyncGetServByPort";
  case 0x006b: return "WSAAsyncGetServByName";
  case 0x006c: return "WSACancelAsyncRequest";
  case 0x006d: return "WSASetBlockingHook";
  case 0x006e: return "WSAUnhookBlockingHook";
  case 0x006f: return "WSAGetLastError";
  case 0x0070: return "WSASetLastError";
  case 0x0071: return "WSACancelBlockingCall";
  case 0x0072: return "WSAIsBlocking";
  case 0x0073: return "WSAStartup";
  case 0x0074: return "WSACleanup";
  case 0x0097: return "__WSAFDIsSet";
  case 0x01f4: return "WEP";
  }
  return nullptr;
}

}
}
}

#endif

