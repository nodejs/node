// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef SRC_NODE_WIN32_ETW_PROVIDER_INL_H_
#define SRC_NODE_WIN32_ETW_PROVIDER_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node_win32_etw_provider.h"
#include "node_etw_provider.h"

namespace node {

// From node_win32_etw_provider.cc
extern REGHANDLE node_provider;
extern EventWriteFunc event_write;
extern int events_enabled;

#define ETW_WRITE_STRING_DATA(data_descriptor, data)                          \
  EventDataDescCreate(data_descriptor,                                        \
                      data,                                                   \
                      (strlen(data) + 1) * sizeof(*data));

#define ETW_WRITE_INT32_DATA(data_descriptor, data)  \
  EventDataDescCreate(data_descriptor, data, sizeof(int32_t));

#define ETW_WRITE_INT64_DATA(data_descriptor, data)  \
  EventDataDescCreate(data_descriptor, data, sizeof(int64_t));

#define ETW_WRITE_ADDRESS_DATA(data_descriptor, data)  \
  EventDataDescCreate(data_descriptor, data, sizeof(intptr_t));

#define ETW_WRITE_INT16_DATA(data_descriptor, data)  \
  EventDataDescCreate(data_descriptor, data, sizeof(int16_t));

#define ETW_WRITE_WSTRING_DATA_LENGTH(data_descriptor, data, data_len_bytes)  \
  EventDataDescCreate(data_descriptor,                                        \
                      data,                                                   \
                      data_len_bytes);

#define ETW_WRITE_NET_CONNECTION(descriptors, conn)                           \
  ETW_WRITE_INT32_DATA(descriptors, &conn->fd);                               \
  ETW_WRITE_INT32_DATA(descriptors + 1, &conn->port);                         \
  ETW_WRITE_STRING_DATA(descriptors + 2, conn->remote);                       \
  ETW_WRITE_INT32_DATA(descriptors + 3, &conn->buffered);

#define ETW_WRITE_HTTP_SERVER_REQUEST(descriptors, req)                       \
  ETW_WRITE_STRING_DATA(descriptors, req->url);                               \
  ETW_WRITE_STRING_DATA(descriptors + 1, req->method);                        \
  ETW_WRITE_STRING_DATA(descriptors + 2, req->forwardedFor);

#define ETW_WRITE_HTTP_CLIENT_REQUEST(descriptors, req)                       \
  ETW_WRITE_STRING_DATA(descriptors, req->url);                               \
  ETW_WRITE_STRING_DATA(descriptors + 1, req->method);

#define ETW_WRITE_GC(descriptors, type, flags)                                \
  ETW_WRITE_INT32_DATA(descriptors, &type);                                   \
  ETW_WRITE_INT32_DATA(descriptors + 1, &flags);

#define ETW_WRITE_V8ADDRESSCHANGE(descriptors, addr1, addr2)                  \
    ETW_WRITE_ADDRESS_DATA(descriptors, &addr1);                              \
    ETW_WRITE_ADDRESS_DATA(descriptors + 1, &addr2);

#define ETW_WRITE_JSMETHOD_LOADUNLOAD(descriptors,                            \
                                      context,                                \
                                      startAddr,                              \
                                      size,                                   \
                                      id,                                     \
                                      flags,                                  \
                                      rangeId,                                \
                                      sourceId,                               \
                                      line,                                   \
                                      col,                                    \
                                      name,                                   \
                                      name_len_bytes)                         \
    ETW_WRITE_ADDRESS_DATA(descriptors, &context);                            \
    ETW_WRITE_ADDRESS_DATA(descriptors + 1, &startAddr);                      \
    ETW_WRITE_INT64_DATA(descriptors + 2, &size);                             \
    ETW_WRITE_INT32_DATA(descriptors + 3, &id);                               \
    ETW_WRITE_INT16_DATA(descriptors + 4, &flags);                            \
    ETW_WRITE_INT16_DATA(descriptors + 5, &rangeId);                          \
    ETW_WRITE_INT64_DATA(descriptors + 6, &sourceId);                         \
    ETW_WRITE_INT32_DATA(descriptors + 7, &line);                             \
    ETW_WRITE_INT32_DATA(descriptors + 8, &col);                              \
    ETW_WRITE_WSTRING_DATA_LENGTH(descriptors + 9, name, name_len_bytes);


#define ETW_WRITE_EVENT(eventDescriptor, dataDescriptors)                     \
  DWORD status = event_write(node_provider,                                   \
                             &eventDescriptor,                                \
                             sizeof(dataDescriptors) /                        \
                                 sizeof(*dataDescriptors),                    \
                             dataDescriptors);                                \
  CHECK_EQ(status, ERROR_SUCCESS);

#define ETW_WRITE_EMPTY_EVENT(eventDescriptor)                                \
  DWORD status = event_write(node_provider,                                   \
                             &eventDescriptor,                                \
                             0,                                               \
                             NULL);  // NOLINT (readability/null_usage)       \
  CHECK_EQ(status, ERROR_SUCCESS);


void NODE_HTTP_SERVER_REQUEST(node_dtrace_http_server_request_t* req,
    node_dtrace_connection_t* conn, const char *remote, int port,
    const char *method, const char *url, int fd) {
  EVENT_DATA_DESCRIPTOR descriptors[7];
  ETW_WRITE_HTTP_SERVER_REQUEST(descriptors, req);
  ETW_WRITE_NET_CONNECTION(descriptors + 3, conn);
  ETW_WRITE_EVENT(NODE_HTTP_SERVER_REQUEST_EVENT, descriptors);
}


void NODE_HTTP_SERVER_RESPONSE(node_dtrace_connection_t* conn,
    const char *remote, int port, int fd) {
  EVENT_DATA_DESCRIPTOR descriptors[4];
  ETW_WRITE_NET_CONNECTION(descriptors, conn);
  ETW_WRITE_EVENT(NODE_HTTP_SERVER_RESPONSE_EVENT, descriptors);
}


void NODE_HTTP_CLIENT_REQUEST(node_dtrace_http_client_request_t* req,
    node_dtrace_connection_t* conn, const char *remote, int port,
    const char *method, const char *url, int fd) {
  EVENT_DATA_DESCRIPTOR descriptors[6];
  ETW_WRITE_HTTP_CLIENT_REQUEST(descriptors, req);
  ETW_WRITE_NET_CONNECTION(descriptors + 2, conn);
  ETW_WRITE_EVENT(NODE_HTTP_CLIENT_REQUEST_EVENT, descriptors);
}


void NODE_HTTP_CLIENT_RESPONSE(node_dtrace_connection_t* conn,
    const char *remote, int port, int fd) {
  EVENT_DATA_DESCRIPTOR descriptors[4];
  ETW_WRITE_NET_CONNECTION(descriptors, conn);
  ETW_WRITE_EVENT(NODE_HTTP_CLIENT_RESPONSE_EVENT, descriptors);
}


void NODE_NET_SERVER_CONNECTION(node_dtrace_connection_t* conn,
    const char *remote, int port, int fd) {
  EVENT_DATA_DESCRIPTOR descriptors[4];
  ETW_WRITE_NET_CONNECTION(descriptors, conn);
  ETW_WRITE_EVENT(NODE_NET_SERVER_CONNECTION_EVENT, descriptors);
}


void NODE_NET_STREAM_END(node_dtrace_connection_t* conn,
    const char *remote, int port, int fd) {
  EVENT_DATA_DESCRIPTOR descriptors[4];
  ETW_WRITE_NET_CONNECTION(descriptors, conn);
  ETW_WRITE_EVENT(NODE_NET_STREAM_END_EVENT, descriptors);
}


void NODE_GC_START(v8::GCType type,
                   v8::GCCallbackFlags flags,
                   v8::Isolate* isolate) {
  if (events_enabled > 0) {
    EVENT_DATA_DESCRIPTOR descriptors[2];
    ETW_WRITE_GC(descriptors, type, flags);
    ETW_WRITE_EVENT(NODE_GC_START_EVENT, descriptors);
  }
}


void NODE_GC_DONE(v8::GCType type,
                  v8::GCCallbackFlags flags,
                  v8::Isolate* isolate) {
  if (events_enabled > 0) {
    EVENT_DATA_DESCRIPTOR descriptors[2];
    ETW_WRITE_GC(descriptors, type, flags);
    ETW_WRITE_EVENT(NODE_GC_DONE_EVENT, descriptors);
  }
}


void NODE_V8SYMBOL_REMOVE(const void* addr1, const void* addr2) {
  if (events_enabled > 0) {
    EVENT_DATA_DESCRIPTOR descriptors[2];
    ETW_WRITE_V8ADDRESSCHANGE(descriptors, addr1, addr2);
    ETW_WRITE_EVENT(NODE_V8SYMBOL_REMOVE_EVENT, descriptors);
  }
}


void NODE_V8SYMBOL_MOVE(const void* addr1, const void* addr2) {
  if (events_enabled > 0) {
    EVENT_DATA_DESCRIPTOR descriptors[2];
    ETW_WRITE_V8ADDRESSCHANGE(descriptors, addr1, addr2);
    ETW_WRITE_EVENT(NODE_V8SYMBOL_MOVE_EVENT, descriptors);
  }
}


void NODE_V8SYMBOL_RESET() {
  if (events_enabled > 0) {
    ETW_WRITE_EMPTY_EVENT(NODE_V8SYMBOL_RESET_EVENT);
  }
}

#define SETSYMBUF(s)  \
  wcscpy(symbuf, s);  \
  symbol_len = arraysize(s) - 1;

void NODE_V8SYMBOL_ADD(LPCSTR symbol,
                       int symbol_len,
                       const void* addr1,
                       int len) {
  if (events_enabled > 0) {
    wchar_t symbuf[128];
    if (symbol == nullptr) {
      SETSYMBUF(L"nullptr");
    } else {
      symbol_len = MultiByteToWideChar(CP_ACP,
                                       0,
                                       symbol,
                                       symbol_len,
                                       symbuf,
                                       128);
      if (symbol_len == 0) {
        SETSYMBUF(L"Invalid");
      } else {
        if (symbol_len > 127) {
          symbol_len = 127;
        }
        symbuf[symbol_len] = L'\0';
      }
    }
    void* context = nullptr;
    INT64 size = (INT64)len;
    INT32 id = (INT32)addr1;
    INT16 flags = 0;
    INT16 rangeid = 1;
    INT32 col = 1;
    INT32 line = 1;
    INT64 sourceid = 0;
    EVENT_DATA_DESCRIPTOR descriptors[10];
    ETW_WRITE_JSMETHOD_LOADUNLOAD(descriptors,
                                  context,
                                  addr1,
                                  size,
                                  id,
                                  flags,
                                  rangeid,
                                  sourceid,
                                  line,
                                  col,
                                  symbuf,
                                  (symbol_len + 1) * sizeof(symbuf[0]));
    ETW_WRITE_EVENT(MethodLoad, descriptors);
  }
}
#undef SETSYMBUF


bool NODE_HTTP_SERVER_REQUEST_ENABLED() { return events_enabled > 0; }
bool NODE_HTTP_SERVER_RESPONSE_ENABLED() { return events_enabled > 0; }
bool NODE_HTTP_CLIENT_REQUEST_ENABLED() { return events_enabled > 0; }
bool NODE_HTTP_CLIENT_RESPONSE_ENABLED() { return events_enabled > 0; }
bool NODE_NET_SERVER_CONNECTION_ENABLED() { return events_enabled > 0; }
bool NODE_NET_STREAM_END_ENABLED() { return events_enabled > 0; }
bool NODE_V8SYMBOL_ENABLED() { return events_enabled > 0; }

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_WIN32_ETW_PROVIDER_INL_H_
