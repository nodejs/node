#include "inspector_socket.h"
#include "util.h"
#include "util-inl.h"

#define NODE_WANT_INTERNALS 1
#include "base64.h"

#include "openssl/sha.h"  // Sha-1 hash

#include <string.h>
#include <vector>

#define ACCEPT_KEY_LENGTH base64_encoded_size(20)
#define BUFFER_GROWTH_CHUNK_SIZE 1024

#define DUMP_READS 0
#define DUMP_WRITES 0

static const char CLOSE_FRAME[] = {'\x88', '\x00'};

enum ws_decode_result {
  FRAME_OK, FRAME_INCOMPLETE, FRAME_CLOSE, FRAME_ERROR
};

#if DUMP_READS || DUMP_WRITES
static void dump_hex(const char* buf, size_t len) {
  const char* ptr = buf;
  const char* end = ptr + len;
  const char* cptr;
  char c;
  int i;

  while (ptr < end) {
    cptr = ptr;
    for (i = 0; i < 16 && ptr < end; i++) {
      printf("%2.2X  ", *(ptr++));
    }
    for (i = 72 - (i * 4); i > 0; i--) {
      printf(" ");
    }
    for (i = 0; i < 16 && cptr < end; i++) {
      c = *(cptr++);
      printf("%c", (c > 0x19) ? c : '.');
    }
    printf("\n");
  }
  printf("\n\n");
}
#endif

static void dispose_inspector(uv_handle_t* handle) {
  inspector_socket_t* inspector =
      reinterpret_cast<inspector_socket_t*>(handle->data);
  inspector_cb close =
      inspector->ws_mode ? inspector->ws_state->close_cb : nullptr;
  inspector->buffer.clear();
  delete inspector->ws_state;
  inspector->ws_state = nullptr;
  inspector->data_len = 0;
  inspector->last_read_end = 0;
  if (close) {
    close(inspector, 0);
  }
}

static void close_connection(inspector_socket_t* inspector) {
  uv_handle_t* socket = reinterpret_cast<uv_handle_t*>(&inspector->client);
  if (!uv_is_closing(socket)) {
    uv_read_stop(reinterpret_cast<uv_stream_t*>(socket));
    uv_close(socket, dispose_inspector);
  }
}

struct WriteRequest {
  WriteRequest(inspector_socket_t* inspector, const char* data, size_t size)
      : inspector(inspector)
      , storage(data, data + size)
      , buf(uv_buf_init(&storage[0], storage.size())) {}

  static WriteRequest* from_write_req(uv_write_t* req) {
    return node::ContainerOf(&WriteRequest::req, req);
  }

  inspector_socket_t* const inspector;
  std::vector<char> storage;
  uv_write_t req;
  uv_buf_t buf;
};

// Cleanup
static void write_request_cleanup(uv_write_t* req, int status) {
  delete WriteRequest::from_write_req(req);
}

static int write_to_client(inspector_socket_t* inspector,
                           const char* msg,
                           size_t len,
                           uv_write_cb write_cb = write_request_cleanup) {
#if DUMP_WRITES
  printf("%s (%ld bytes):\n", __FUNCTION__, len);
  dump_hex(msg, len);
#endif

  // Freed in write_request_cleanup
  WriteRequest* wr = new WriteRequest(inspector, msg, len);
  uv_stream_t* stream = reinterpret_cast<uv_stream_t*>(&inspector->client);
  return uv_write(&wr->req, stream, &wr->buf, 1, write_cb) < 0;
}

// Constants for hybi-10 frame format.

typedef int OpCode;

const OpCode kOpCodeContinuation = 0x0;
const OpCode kOpCodeText = 0x1;
const OpCode kOpCodeBinary = 0x2;
const OpCode kOpCodeClose = 0x8;
const OpCode kOpCodePing = 0x9;
const OpCode kOpCodePong = 0xA;

const unsigned char kFinalBit = 0x80;
const unsigned char kReserved1Bit = 0x40;
const unsigned char kReserved2Bit = 0x20;
const unsigned char kReserved3Bit = 0x10;
const unsigned char kOpCodeMask = 0xF;
const unsigned char kMaskBit = 0x80;
const unsigned char kPayloadLengthMask = 0x7F;

const size_t kMaxSingleBytePayloadLength = 125;
const size_t kTwoBytePayloadLengthField = 126;
const size_t kEightBytePayloadLengthField = 127;
const size_t kMaskingKeyWidthInBytes = 4;

static std::vector<char> encode_frame_hybi17(const char* message,
                                             size_t data_length) {
  std::vector<char> frame;
  OpCode op_code = kOpCodeText;
  frame.push_back(kFinalBit | op_code);
  if (data_length <= kMaxSingleBytePayloadLength) {
    frame.push_back(static_cast<char>(data_length));
  } else if (data_length <= 0xFFFF) {
    frame.push_back(kTwoBytePayloadLengthField);
    frame.push_back((data_length & 0xFF00) >> 8);
    frame.push_back(data_length & 0xFF);
  } else {
    frame.push_back(kEightBytePayloadLengthField);
    char extended_payload_length[8];
    size_t remaining = data_length;
    // Fill the length into extended_payload_length in the network byte order.
    for (int i = 0; i < 8; ++i) {
      extended_payload_length[7 - i] = remaining & 0xFF;
      remaining >>= 8;
    }
    frame.insert(frame.end(), extended_payload_length,
                 extended_payload_length + 8);
    ASSERT_EQ(0, remaining);
  }
  frame.insert(frame.end(), message, message + data_length);
  return frame;
}

static ws_decode_result decode_frame_hybi17(const char* buffer_begin,
                                            size_t data_length,
                                            bool client_frame,
                                            int* bytes_consumed,
                                            std::vector<char>* output,
                                            bool* compressed) {
  *bytes_consumed = 0;
  if (data_length < 2)
    return FRAME_INCOMPLETE;

  const char* p = buffer_begin;
  const char* buffer_end = p + data_length;

  unsigned char first_byte = *p++;
  unsigned char second_byte = *p++;

  bool final = (first_byte & kFinalBit) != 0;
  bool reserved1 = (first_byte & kReserved1Bit) != 0;
  bool reserved2 = (first_byte & kReserved2Bit) != 0;
  bool reserved3 = (first_byte & kReserved3Bit) != 0;
  int op_code = first_byte & kOpCodeMask;
  bool masked = (second_byte & kMaskBit) != 0;
  *compressed = reserved1;
  if (!final || reserved2 || reserved3)
    return FRAME_ERROR;  // Only compression extension is supported.

  bool closed = false;
  switch (op_code) {
    case kOpCodeClose:
      closed = true;
      break;
    case kOpCodeText:
      break;
    case kOpCodeBinary:        // We don't support binary frames yet.
    case kOpCodeContinuation:  // We don't support binary frames yet.
    case kOpCodePing:          // We don't support binary frames yet.
    case kOpCodePong:          // We don't support binary frames yet.
    default:
      return FRAME_ERROR;
  }

  // In Hybi-17 spec client MUST mask its frame.
  if (client_frame && !masked) {
    return FRAME_ERROR;
  }

  uint64_t payload_length64 = second_byte & kPayloadLengthMask;
  if (payload_length64 > kMaxSingleBytePayloadLength) {
    int extended_payload_length_size;
    if (payload_length64 == kTwoBytePayloadLengthField) {
      extended_payload_length_size = 2;
    } else if (payload_length64 == kEightBytePayloadLengthField) {
      extended_payload_length_size = 8;
    } else {
      return FRAME_ERROR;
    }
    if (buffer_end - p < extended_payload_length_size)
      return FRAME_INCOMPLETE;
    payload_length64 = 0;
    for (int i = 0; i < extended_payload_length_size; ++i) {
      payload_length64 <<= 8;
      payload_length64 |= static_cast<unsigned char>(*p++);
    }
  }

  static const uint64_t max_payload_length = 0x7FFFFFFFFFFFFFFFull;
  static const size_t max_length = SIZE_MAX;
  if (payload_length64 > max_payload_length ||
      payload_length64 > max_length - kMaskingKeyWidthInBytes) {
    // WebSocket frame length too large.
    return FRAME_ERROR;
  }
  size_t payload_length = static_cast<size_t>(payload_length64);

  if (data_length - kMaskingKeyWidthInBytes < payload_length)
    return FRAME_INCOMPLETE;

  const char* masking_key = p;
  const char* payload = p + kMaskingKeyWidthInBytes;
  for (size_t i = 0; i < payload_length; ++i)  // Unmask the payload.
    output->insert(output->end(),
                   payload[i] ^ masking_key[i % kMaskingKeyWidthInBytes]);

  size_t pos = p + kMaskingKeyWidthInBytes + payload_length - buffer_begin;
  *bytes_consumed = pos;
  return closed ? FRAME_CLOSE : FRAME_OK;
}

static void invoke_read_callback(inspector_socket_t* inspector,
                                 int status, const uv_buf_t* buf) {
  if (inspector->ws_state->read_cb) {
    inspector->ws_state->read_cb(
        reinterpret_cast<uv_stream_t*>(&inspector->client), status, buf);
  }
}

static void shutdown_complete(inspector_socket_t* inspector) {
  close_connection(inspector);
}

static void on_close_frame_written(uv_write_t* req, int status) {
  WriteRequest* wr = WriteRequest::from_write_req(req);
  inspector_socket_t* inspector = wr->inspector;
  delete wr;
  inspector->ws_state->close_sent = true;
  if (inspector->ws_state->received_close) {
    shutdown_complete(inspector);
  }
}

static void close_frame_received(inspector_socket_t* inspector) {
  inspector->ws_state->received_close = true;
  if (!inspector->ws_state->close_sent) {
    invoke_read_callback(inspector, 0, 0);
    write_to_client(inspector, CLOSE_FRAME, sizeof(CLOSE_FRAME),
                    on_close_frame_written);
  } else {
    shutdown_complete(inspector);
  }
}

static int parse_ws_frames(inspector_socket_t* inspector, size_t len) {
  int bytes_consumed = 0;
  std::vector<char> output;
  bool compressed = false;

  ws_decode_result r =  decode_frame_hybi17(&inspector->buffer[0],
                                            len, true /* client_frame */,
                                            &bytes_consumed, &output,
                                            &compressed);
  // Compressed frame means client is ignoring the headers and misbehaves
  if (compressed || r == FRAME_ERROR) {
    invoke_read_callback(inspector, UV_EPROTO, nullptr);
    close_connection(inspector);
    bytes_consumed = 0;
  } else if (r == FRAME_CLOSE) {
    close_frame_received(inspector);
    bytes_consumed = 0;
  } else if (r == FRAME_OK && inspector->ws_state->alloc_cb
             && inspector->ws_state->read_cb) {
    uv_buf_t buffer;
    size_t len = output.size();
    inspector->ws_state->alloc_cb(
        reinterpret_cast<uv_handle_t*>(&inspector->client),
        len, &buffer);
    CHECK_GE(buffer.len, len);
    memcpy(buffer.base, &output[0], len);
    invoke_read_callback(inspector, len, &buffer);
  }
  return bytes_consumed;
}

static void prepare_buffer(uv_handle_t* stream, size_t len, uv_buf_t* buf) {
  inspector_socket_t* inspector =
      reinterpret_cast<inspector_socket_t*>(stream->data);

  if (len > (inspector->buffer.size() - inspector->data_len)) {
    int new_size = (inspector->data_len + len + BUFFER_GROWTH_CHUNK_SIZE - 1) /
                   BUFFER_GROWTH_CHUNK_SIZE *
                   BUFFER_GROWTH_CHUNK_SIZE;
    inspector->buffer.resize(new_size);
  }
  buf->base = &inspector->buffer[inspector->data_len];
  buf->len = len;
  inspector->data_len += len;
}

static void websockets_data_cb(uv_stream_t* stream, ssize_t nread,
                               const uv_buf_t* buf) {
  inspector_socket_t* inspector =
      reinterpret_cast<inspector_socket_t*>(stream->data);
  if (nread < 0 || nread == UV_EOF) {
    inspector->connection_eof = true;
    if (!inspector->shutting_down && inspector->ws_state->read_cb) {
      inspector->ws_state->read_cb(stream, nread, nullptr);
    }
  } else {
    #if DUMP_READS
      printf("%s read %ld bytes\n", __FUNCTION__, nread);
      if (nread > 0) {
        dump_hex(buf->base, nread);
      }
    #endif
    // 1. Move read bytes to continue the buffer
    // Should be same as this is supposedly last buffer
    ASSERT_EQ(buf->base + buf->len, &inspector->buffer[inspector->data_len]);

    // Should be noop...
    memmove(&inspector->buffer[inspector->last_read_end], buf->base, nread);
    inspector->last_read_end += nread;

    // 2. Parse.
    int processed = 0;
    do {
      processed = parse_ws_frames(inspector, inspector->last_read_end);
      // 3. Fix the buffer size & length
      if (processed > 0) {
        memmove(&inspector->buffer[0], &inspector->buffer[processed],
                inspector->last_read_end - processed);
        inspector->last_read_end -= processed;
        inspector->data_len = inspector->last_read_end;
      }
    } while (processed > 0 && inspector->data_len > 0);
  }
}

int inspector_read_start(inspector_socket_t* inspector,
                          uv_alloc_cb alloc_cb, uv_read_cb read_cb) {
  ASSERT(inspector->ws_mode);
  ASSERT(!inspector->shutting_down || read_cb == nullptr);
  inspector->ws_state->close_sent = false;
  inspector->ws_state->alloc_cb = alloc_cb;
  inspector->ws_state->read_cb = read_cb;
  int err =
      uv_read_start(reinterpret_cast<uv_stream_t*>(&inspector->client),
                    prepare_buffer,
                    websockets_data_cb);
  if (err < 0) {
    close_connection(inspector);
  }
  return err;
}

void inspector_read_stop(inspector_socket_t* inspector) {
  uv_read_stop(reinterpret_cast<uv_stream_t*>(&inspector->client));
  inspector->ws_state->alloc_cb = nullptr;
  inspector->ws_state->read_cb = nullptr;
}

static void generate_accept_string(const std::string& client_key,
                                   char (*buffer)[ACCEPT_KEY_LENGTH]) {
  // Magic string from websockets spec.
  static const char ws_magic[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  std::string input(client_key + ws_magic);
  char hash[SHA_DIGEST_LENGTH];
  SHA1(reinterpret_cast<const unsigned char*>(&input[0]), input.size(),
       reinterpret_cast<unsigned char*>(hash));
  node::base64_encode(hash, sizeof(hash), *buffer, sizeof(*buffer));
}

static int header_value_cb(http_parser* parser, const char* at, size_t length) {
  static const char SEC_WEBSOCKET_KEY_HEADER[] = "Sec-WebSocket-Key";
  auto inspector = static_cast<inspector_socket_t*>(parser->data);
  auto state = inspector->http_parsing_state;
  state->parsing_value = true;
  if (state->current_header.size() == sizeof(SEC_WEBSOCKET_KEY_HEADER) - 1 &&
      node::StringEqualNoCaseN(state->current_header.data(),
                               SEC_WEBSOCKET_KEY_HEADER,
                               sizeof(SEC_WEBSOCKET_KEY_HEADER) - 1)) {
    state->ws_key.append(at, length);
  }
  return 0;
}

static int header_field_cb(http_parser* parser, const char* at, size_t length) {
  auto inspector = static_cast<inspector_socket_t*>(parser->data);
  auto state = inspector->http_parsing_state;
  if (state->parsing_value) {
    state->parsing_value = false;
    state->current_header.clear();
  }
  state->current_header.append(at, length);
  return 0;
}

static int path_cb(http_parser* parser, const char* at, size_t length) {
  auto inspector = static_cast<inspector_socket_t*>(parser->data);
  auto state = inspector->http_parsing_state;
  state->path.append(at, length);
  return 0;
}

static void handshake_complete(inspector_socket_t* inspector) {
  uv_read_stop(reinterpret_cast<uv_stream_t*>(&inspector->client));
  handshake_cb callback = inspector->http_parsing_state->callback;
  inspector->ws_state = new ws_state_s();
  inspector->last_read_end = 0;
  inspector->ws_mode = true;
  callback(inspector, kInspectorHandshakeUpgraded,
           inspector->http_parsing_state->path);
}

static void cleanup_http_parsing_state(inspector_socket_t* inspector) {
  delete inspector->http_parsing_state;
  inspector->http_parsing_state = nullptr;
}

static void report_handshake_failure_cb(uv_handle_t* handle) {
  dispose_inspector(handle);
  inspector_socket_t* inspector =
      static_cast<inspector_socket_t*>(handle->data);
  handshake_cb cb = inspector->http_parsing_state->callback;
  cleanup_http_parsing_state(inspector);
  cb(inspector, kInspectorHandshakeFailed, std::string());
}

static void close_and_report_handshake_failure(inspector_socket_t* inspector) {
  uv_handle_t* socket = reinterpret_cast<uv_handle_t*>(&inspector->client);
  if (uv_is_closing(socket)) {
    report_handshake_failure_cb(socket);
  } else {
    uv_read_stop(reinterpret_cast<uv_stream_t*>(socket));
    uv_close(socket, report_handshake_failure_cb);
  }
}

static void handshake_failed(inspector_socket_t* inspector) {
  const char HANDSHAKE_FAILED_RESPONSE[] =
      "HTTP/1.0 400 Bad Request\r\n"
      "Content-Type: text/html; charset=UTF-8\r\n\r\n"
      "WebSockets request was expected\r\n";
  write_to_client(inspector, HANDSHAKE_FAILED_RESPONSE,
                  sizeof(HANDSHAKE_FAILED_RESPONSE) - 1);
  close_and_report_handshake_failure(inspector);
}

// init_handshake references message_complete_cb
static void init_handshake(inspector_socket_t* inspector);

static int message_complete_cb(http_parser* parser) {
  inspector_socket_t* inspector =
      reinterpret_cast<inspector_socket_t*>(parser->data);
  struct http_parsing_state_s* state =
      (struct http_parsing_state_s*) inspector->http_parsing_state;
  if (parser->method != HTTP_GET) {
    handshake_failed(inspector);
  } else if (!parser->upgrade) {
    if (state->callback(inspector, kInspectorHandshakeHttpGet, state->path)) {
      init_handshake(inspector);
    } else {
      handshake_failed(inspector);
    }
  } else if (state->ws_key.empty()) {
    handshake_failed(inspector);
  } else if (state->callback(inspector, kInspectorHandshakeUpgrading,
                             state->path)) {
    char accept_string[ACCEPT_KEY_LENGTH];
    generate_accept_string(state->ws_key, &accept_string);
    const char accept_ws_prefix[] = "HTTP/1.1 101 Switching Protocols\r\n"
                                    "Upgrade: websocket\r\n"
                                    "Connection: Upgrade\r\n"
                                    "Sec-WebSocket-Accept: ";
    const char accept_ws_suffix[] = "\r\n\r\n";
    std::string reply(accept_ws_prefix, sizeof(accept_ws_prefix) - 1);
    reply.append(accept_string, sizeof(accept_string));
    reply.append(accept_ws_suffix, sizeof(accept_ws_suffix) - 1);
    if (write_to_client(inspector, &reply[0], reply.size()) >= 0) {
      handshake_complete(inspector);
      inspector->http_parsing_state->done = true;
    } else {
      close_and_report_handshake_failure(inspector);
    }
  } else {
    handshake_failed(inspector);
  }
  return 0;
}

static void data_received_cb(uv_stream_s* client, ssize_t nread,
                             const uv_buf_t* buf) {
#if DUMP_READS
  if (nread >= 0) {
    printf("%s (%ld bytes)\n", __FUNCTION__, nread);
    dump_hex(buf->base, nread);
  } else {
    printf("[%s:%d] %s\n", __FUNCTION__, __LINE__, uv_err_name(nread));
  }
#endif
  inspector_socket_t* inspector =
      reinterpret_cast<inspector_socket_t*>((client->data));
  if (nread < 0 || nread == UV_EOF) {
    close_and_report_handshake_failure(inspector);
  } else {
    http_parsing_state_s* state = inspector->http_parsing_state;
    http_parser* parser = &state->parser;
    http_parser_execute(parser, &state->parser_settings, &inspector->buffer[0],
                        nread);
    if (parser->http_errno != HPE_OK) {
      handshake_failed(inspector);
    }
    if (inspector->http_parsing_state->done) {
      cleanup_http_parsing_state(inspector);
    }
    inspector->data_len = 0;
  }
}

static void init_handshake(inspector_socket_t* inspector) {
  http_parsing_state_s* state = inspector->http_parsing_state;
  CHECK_NE(state, nullptr);
  state->current_header.clear();
  state->ws_key.clear();
  state->path.clear();
  state->done = false;
  http_parser_init(&state->parser, HTTP_REQUEST);
  state->parser.data = inspector;
  http_parser_settings* settings = &state->parser_settings;
  http_parser_settings_init(settings);
  settings->on_header_field = header_field_cb;
  settings->on_header_value = header_value_cb;
  settings->on_message_complete = message_complete_cb;
  settings->on_url = path_cb;
}

int inspector_accept(uv_stream_t* server, inspector_socket_t* inspector,
                     handshake_cb callback) {
  ASSERT_NE(callback, nullptr);
  CHECK_EQ(inspector->http_parsing_state, nullptr);
  inspector->http_parsing_state = new http_parsing_state_s();
  uv_stream_t* client = reinterpret_cast<uv_stream_t*>(&inspector->client);
  CHECK_NE(client, nullptr);
  int err = uv_tcp_init(server->loop, &inspector->client);

  if (err == 0) {
    err = uv_accept(server, client);
  }
  if (err == 0) {
    client->data = inspector;
    init_handshake(inspector);
    inspector->http_parsing_state->callback = callback;
    err = uv_read_start(client, prepare_buffer,
                        data_received_cb);
  }
  if (err != 0) {
    uv_close(reinterpret_cast<uv_handle_t*>(client), NULL);
  }
  return err;
}

void inspector_write(inspector_socket_t* inspector, const char* data,
                     size_t len) {
  if (inspector->ws_mode) {
    std::vector<char> output = encode_frame_hybi17(data, len);
    write_to_client(inspector, &output[0], output.size());
  } else {
    write_to_client(inspector, data, len);
  }
}

void inspector_close(inspector_socket_t* inspector,
    inspector_cb callback) {
  // libuv throws assertions when closing stream that's already closed - we
  // need to do the same.
  ASSERT(!uv_is_closing(reinterpret_cast<uv_handle_t*>(&inspector->client)));
  ASSERT(!inspector->shutting_down);
  inspector->shutting_down = true;
  inspector->ws_state->close_cb = callback;
  if (inspector->connection_eof) {
    close_connection(inspector);
  } else {
    inspector_read_stop(inspector);
    write_to_client(inspector, CLOSE_FRAME, sizeof(CLOSE_FRAME),
                    on_close_frame_written);
    inspector_read_start(inspector, nullptr, nullptr);
  }
}

bool inspector_is_active(const struct inspector_socket_s* inspector) {
  const uv_handle_t* client =
      reinterpret_cast<const uv_handle_t*>(&inspector->client);
  return !inspector->shutting_down && !uv_is_closing(client);
}
