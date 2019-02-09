#include "node_internals.h"
#include "node_report.h"

namespace report {

using node::MallocedBuffer;

static constexpr auto null = JSONWriter::Null{};

// Utility function to format libuv socket information.
static void ReportEndpoints(uv_handle_t* h, JSONWriter* writer) {
  struct sockaddr_storage addr_storage;
  struct sockaddr* addr = reinterpret_cast<sockaddr*>(&addr_storage);
  uv_any_handle* handle = reinterpret_cast<uv_any_handle*>(h);
  int addr_size = sizeof(addr_storage);
  int rc = -1;
  bool wrote_local_endpoint = false;
  bool wrote_remote_endpoint = false;

  switch (h->type) {
    case UV_UDP:
      rc = uv_udp_getsockname(&handle->udp, addr, &addr_size);
      break;
    case UV_TCP:
      rc = uv_tcp_getsockname(&handle->tcp, addr, &addr_size);
      break;
    default:
      break;
  }
  if (rc == 0) {
    // uv_getnameinfo will format host and port and handle IPv4/IPv6.
    uv_getnameinfo_t local;
    rc = uv_getnameinfo(h->loop, &local, nullptr, addr, NI_NUMERICSERV);

    if (rc == 0) {
      writer->json_objectstart("localEndpoint");
      writer->json_keyvalue("host", local.host);
      writer->json_keyvalue("port", local.service);
      writer->json_objectend();
      wrote_local_endpoint = true;
    }
  }
  if (!wrote_local_endpoint) writer->json_keyvalue("localEndpoint", null);

  if (h->type == UV_TCP) {
    // Get the remote end of the connection.
    rc = uv_tcp_getpeername(&handle->tcp, addr, &addr_size);
    if (rc == 0) {
      uv_getnameinfo_t remote;
      rc = uv_getnameinfo(h->loop, &remote, nullptr, addr, NI_NUMERICSERV);

      if (rc == 0) {
        writer->json_objectstart("remoteEndpoint");
        writer->json_keyvalue("host", remote.host);
        writer->json_keyvalue("port", remote.service);
        writer->json_objectend();
        wrote_local_endpoint = true;
      }
    }
  }
  if (!wrote_remote_endpoint) writer->json_keyvalue("remoteEndpoint", null);
}

// Utility function to format libuv path information.
static void ReportPath(uv_handle_t* h, JSONWriter* writer) {
  MallocedBuffer<char> buffer(0);
  int rc = -1;
  size_t size = 0;
  uv_any_handle* handle = reinterpret_cast<uv_any_handle*>(h);
  bool wrote_filename = false;
  // First call to get required buffer size.
  switch (h->type) {
    case UV_FS_EVENT:
      rc = uv_fs_event_getpath(&(handle->fs_event), buffer.data, &size);
      break;
    case UV_FS_POLL:
      rc = uv_fs_poll_getpath(&(handle->fs_poll), buffer.data, &size);
      break;
    default:
      break;
  }
  if (rc == UV_ENOBUFS) {
    buffer = MallocedBuffer<char>(size + 1);
    switch (h->type) {
      case UV_FS_EVENT:
        rc = uv_fs_event_getpath(&(handle->fs_event), buffer.data, &size);
        break;
      case UV_FS_POLL:
        rc = uv_fs_poll_getpath(&(handle->fs_poll), buffer.data, &size);
        break;
      default:
        break;
    }
    if (rc == 0) {
      // buffer is not null terminated.
      buffer.data[size] = '\0';
      writer->json_keyvalue("filename", buffer.data);
      wrote_filename = true;
    }
  }
  if (!wrote_filename) writer->json_keyvalue("filename", null);
}

// Utility function to walk libuv handles.
void WalkHandle(uv_handle_t* h, void* arg) {
  const char* type = uv_handle_type_name(h->type);
  JSONWriter* writer = static_cast<JSONWriter*>(arg);
  uv_any_handle* handle = reinterpret_cast<uv_any_handle*>(h);

  writer->json_start();

  switch (h->type) {
    case UV_FS_EVENT:
    case UV_FS_POLL:
      ReportPath(h, writer);
      break;
    case UV_PROCESS:
      writer->json_keyvalue("pid", handle->process.pid);
      break;
    case UV_TCP:
    case UV_UDP:
      ReportEndpoints(h, writer);
      break;
    case UV_TIMER: {
      uint64_t due = handle->timer.timeout;
      uint64_t now = uv_now(handle->timer.loop);
      writer->json_keyvalue("repeat", uv_timer_get_repeat(&handle->timer));
      writer->json_keyvalue("firesInMsFromNow",
                            static_cast<int64_t>(due - now));
      writer->json_keyvalue("expired", now >= due);
      break;
    }
    case UV_TTY: {
      int height, width, rc;
      rc = uv_tty_get_winsize(&(handle->tty), &width, &height);
      if (rc == 0) {
        writer->json_keyvalue("width", width);
        writer->json_keyvalue("height", height);
      }
      break;
    }
    case UV_SIGNAL:
      // SIGWINCH is used by libuv so always appears.
      // See http://docs.libuv.org/en/v1.x/signal.html
      writer->json_keyvalue("signum", handle->signal.signum);
      writer->json_keyvalue("signal",
                            node::signo_string(handle->signal.signum));
      break;
    default:
      break;
  }

  if (h->type == UV_TCP || h->type == UV_UDP
#ifndef _WIN32
      || h->type == UV_NAMED_PIPE
#endif
  ) {
    // These *must* be 0 or libuv will set the buffer sizes to the non-zero
    // values they contain.
    int send_size = 0;
    int recv_size = 0;
    uv_send_buffer_size(h, &send_size);
    uv_recv_buffer_size(h, &recv_size);
    writer->json_keyvalue("sendBufferSize", send_size);
    writer->json_keyvalue("recvBufferSize", recv_size);
  }

#ifndef _WIN32
  if (h->type == UV_TCP || h->type == UV_NAMED_PIPE || h->type == UV_TTY ||
      h->type == UV_UDP || h->type == UV_POLL) {
    uv_os_fd_t fd_v;
    int rc = uv_fileno(h, &fd_v);

    if (rc == 0) {
      writer->json_keyvalue("fd", static_cast<int>(fd_v));
      switch (fd_v) {
        case 0:
          writer->json_keyvalue("stdio", "stdin");
          break;
        case 1:
          writer->json_keyvalue("stdio", "stdout");
          break;
        case 2:
          writer->json_keyvalue("stdio", "stderr");
          break;
      }
    }
  }
#endif

  if (h->type == UV_TCP || h->type == UV_NAMED_PIPE || h->type == UV_TTY) {
    writer->json_keyvalue("writeQueueSize", handle->stream.write_queue_size);
    writer->json_keyvalue("readable",
                          static_cast<bool>(uv_is_readable(&handle->stream)));
    writer->json_keyvalue("writable",
                          static_cast<bool>(uv_is_writable(&handle->stream)));
  }

  writer->json_keyvalue("type", type);
  writer->json_keyvalue("is_active", static_cast<bool>(uv_is_active(h)));
  writer->json_keyvalue("is_referenced", static_cast<bool>(uv_has_ref(h)));
  writer->json_keyvalue("address",
                        ValueToHexString(reinterpret_cast<uint64_t>(h)));
  writer->json_end();
}

std::string EscapeJsonChars(const std::string& str) {
  const std::string control_symbols[0x20] = {
      "\\u0000", "\\u0001", "\\u0002", "\\u0003", "\\u0004", "\\u0005",
      "\\u0006", "\\u0007", "\\b", "\\t", "\\n", "\\v", "\\f", "\\r",
      "\\u000e", "\\u000f", "\\u0010", "\\u0011", "\\u0012", "\\u0013",
      "\\u0014", "\\u0015", "\\u0016", "\\u0017", "\\u0018", "\\u0019",
      "\\u001a", "\\u001b", "\\u001c", "\\u001d", "\\u001e", "\\u001f"
  };

  std::string ret = "";
  size_t last_pos = 0;
  size_t pos = 0;
  for (; pos < str.size(); ++pos) {
    std::string replace;
    char ch = str[pos];
    if (ch == '\\') {
      replace = "\\\\";
    } else if (ch == '\"') {
      replace = "\\\"";
    } else {
      size_t num = static_cast<size_t>(ch);
      if (num < 0x20) replace = control_symbols[num];
    }
    if (!replace.empty()) {
      if (pos > last_pos) {
        ret += str.substr(last_pos, pos - last_pos);
      }
      last_pos = pos + 1;
      ret += replace;
    }
  }
  // Append any remaining symbols.
  if (last_pos < str.size()) {
    ret += str.substr(last_pos, pos - last_pos);
  }
  return ret;
}

}  // namespace report
