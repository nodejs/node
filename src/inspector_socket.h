#ifndef SRC_INSPECTOR_SOCKET_H_
#define SRC_INSPECTOR_SOCKET_H_

#include "http_parser.h"
#include "uv.h"

#include <string>
#include <vector>

enum inspector_handshake_event {
  kInspectorHandshakeUpgrading,
  kInspectorHandshakeUpgraded,
  kInspectorHandshakeHttpGet,
  kInspectorHandshakeFailed
};

struct inspector_socket_s;

typedef void (*inspector_cb)(struct inspector_socket_s*, int);
// Notifies as handshake is progressing. Returning false as a response to
// kInspectorHandshakeUpgrading or kInspectorHandshakeHttpGet event will abort
// the connection. inspector_write can be used from the callback.
typedef bool (*handshake_cb)(struct inspector_socket_s*,
                             enum inspector_handshake_event state,
                             const std::string& path);

struct http_parsing_state_s {
  http_parser parser;
  http_parser_settings parser_settings;
  handshake_cb callback;
  bool done;
  bool parsing_value;
  std::string ws_key;
  std::string path;
  std::string current_header;
};

struct ws_state_s {
  uv_alloc_cb alloc_cb;
  uv_read_cb read_cb;
  inspector_cb close_cb;
  bool close_sent;
  bool received_close;
};

struct inspector_socket_s {
  void* data;
  struct http_parsing_state_s* http_parsing_state;
  struct ws_state_s* ws_state;
  std::vector<char> buffer;
  size_t data_len;
  size_t last_read_end;
  uv_tcp_t client;
  bool ws_mode;
  bool shutting_down;
  bool connection_eof;
};

typedef struct inspector_socket_s inspector_socket_t;

int inspector_accept(uv_stream_t* server, struct inspector_socket_s* inspector,
                     handshake_cb callback);

void inspector_close(struct inspector_socket_s* inspector,
                     inspector_cb callback);

// Callbacks will receive handles that has inspector in data field...
int inspector_read_start(struct inspector_socket_s* inspector, uv_alloc_cb,
                          uv_read_cb);
void inspector_read_stop(struct inspector_socket_s* inspector);
void inspector_write(struct inspector_socket_s* inspector,
    const char* data, size_t len);
bool inspector_is_active(const struct inspector_socket_s* inspector);

#endif  // SRC_INSPECTOR_SOCKET_H_
