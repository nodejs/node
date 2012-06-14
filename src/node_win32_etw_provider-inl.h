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

#ifndef SRC_ETW_INL_H_
#define SRC_ETW_INL_H_

#include "node_win32_etw_provider.h"
#include "node_etw_provider.h"

namespace node {

using namespace v8;

// From node_win32_etw_provider.cc
extern REGHANDLE node_provider;
extern EventWriteFunc event_write;
extern int events_enabled;

#define ETW_WRITE_STRING_DATA(data_descriptor, data)                          \
  EventDataDescCreate(data_descriptor,                                        \
                      data,                                                   \
                      (strlen(data) + 1) * sizeof(char));

#define ETW_WRITE_INT32_DATA(data_descriptor, data)  \
  EventDataDescCreate(data_descriptor, data, sizeof(int32_t));

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

#define ETW_WRITE_EVENT(eventDescriptor, dataDescriptors)                     \
  DWORD status = event_write(node_provider,                                   \
                             &eventDescriptor,                                \
                             sizeof(dataDescriptors)/sizeof(*dataDescriptors),\
                             dataDescriptors);                                \
  assert(status == ERROR_SUCCESS);


void NODE_HTTP_SERVER_REQUEST(node_dtrace_http_server_request_t* req,
    node_dtrace_connection_t* conn) {
  EVENT_DATA_DESCRIPTOR descriptors[7];
  ETW_WRITE_HTTP_SERVER_REQUEST(descriptors, req);
  ETW_WRITE_NET_CONNECTION(descriptors + 3, conn);
  ETW_WRITE_EVENT(NODE_HTTP_SERVER_REQUEST_EVENT, descriptors);
}


void NODE_HTTP_SERVER_RESPONSE(node_dtrace_connection_t* conn) {
  EVENT_DATA_DESCRIPTOR descriptors[4];
  ETW_WRITE_NET_CONNECTION(descriptors, conn);
  ETW_WRITE_EVENT(NODE_HTTP_SERVER_RESPONSE_EVENT, descriptors);
}


void NODE_HTTP_CLIENT_REQUEST(node_dtrace_http_client_request_t* req,
    node_dtrace_connection_t* conn) {
  EVENT_DATA_DESCRIPTOR descriptors[6];
  ETW_WRITE_HTTP_CLIENT_REQUEST(descriptors, req);
  ETW_WRITE_NET_CONNECTION(descriptors + 2, conn);
  ETW_WRITE_EVENT(NODE_HTTP_CLIENT_REQUEST_EVENT, descriptors);
}


void NODE_HTTP_CLIENT_RESPONSE(node_dtrace_connection_t* conn) {
  EVENT_DATA_DESCRIPTOR descriptors[4];
  ETW_WRITE_NET_CONNECTION(descriptors, conn);
  ETW_WRITE_EVENT(NODE_HTTP_CLIENT_RESPONSE_EVENT, descriptors);
}


void NODE_NET_SERVER_CONNECTION(node_dtrace_connection_t* conn) {
  EVENT_DATA_DESCRIPTOR descriptors[4];
  ETW_WRITE_NET_CONNECTION(descriptors, conn);
  ETW_WRITE_EVENT(NODE_NET_SERVER_CONNECTION_EVENT, descriptors);
}


void NODE_NET_STREAM_END(node_dtrace_connection_t* conn) {
  EVENT_DATA_DESCRIPTOR descriptors[4];
  ETW_WRITE_NET_CONNECTION(descriptors, conn);
  ETW_WRITE_EVENT(NODE_NET_STREAM_END_EVENT, descriptors);
}


void NODE_GC_START(GCType type, GCCallbackFlags flags) {
  if (events_enabled > 0) {
    EVENT_DATA_DESCRIPTOR descriptors[2];
    ETW_WRITE_GC(descriptors, type, flags);
    ETW_WRITE_EVENT(NODE_GC_START_EVENT, descriptors);
  }
}


void NODE_GC_DONE(GCType type, GCCallbackFlags flags) {
  if (events_enabled > 0) {
    EVENT_DATA_DESCRIPTOR descriptors[2];
    ETW_WRITE_GC(descriptors, type, flags);
    ETW_WRITE_EVENT(NODE_GC_DONE_EVENT, descriptors);
  }
}


bool NODE_HTTP_SERVER_REQUEST_ENABLED() { return events_enabled > 0; }
bool NODE_HTTP_SERVER_RESPONSE_ENABLED() { return events_enabled > 0; }
bool NODE_HTTP_CLIENT_REQUEST_ENABLED() { return events_enabled > 0; }
bool NODE_HTTP_CLIENT_RESPONSE_ENABLED() { return events_enabled > 0; }
bool NODE_NET_SERVER_CONNECTION_ENABLED() { return events_enabled > 0; }
bool NODE_NET_STREAM_END_ENABLED() { return events_enabled > 0; }
bool NODE_NET_SOCKET_READ_ENABLED() { return events_enabled > 0; }
bool NODE_NET_SOCKET_WRITE_ENABLED() { return events_enabled > 0; }
}
#endif  // SRC_ETW_INL_H_