#include "node_internals.h"
#include "node_report.h"

namespace report {

using node::MallocedBuffer;

// Utility function to format libuv socket information.
void ReportEndpoints(uv_handle_t* h, std::ostringstream& out) {
  struct sockaddr_storage addr_storage;
  struct sockaddr* addr = reinterpret_cast<sockaddr*>(&addr_storage);
  char hostbuf[NI_MAXHOST];
  char portbuf[NI_MAXSERV];
  uv_any_handle* handle = reinterpret_cast<uv_any_handle*>(h);
  int addr_size = sizeof(addr_storage);
  int rc = -1;

  switch (h->type) {
    case UV_UDP: {
      rc = uv_udp_getsockname(&(handle->udp), addr, &addr_size);
      break;
    }
    case UV_TCP: {
      rc = uv_tcp_getsockname(&(handle->tcp), addr, &addr_size);
      break;
    }
    default:
      break;
  }
  if (rc == 0) {
    // getnameinfo will format host and port and handle IPv4/IPv6.
    rc = getnameinfo(addr,
                     addr_size,
                     hostbuf,
                     sizeof(hostbuf),
                     portbuf,
                     sizeof(portbuf),
                     NI_NUMERICSERV);
    if (rc == 0) {
      out << std::string(hostbuf) << ":" << std::string(portbuf);
    }

    if (h->type == UV_TCP) {
      // Get the remote end of the connection.
      rc = uv_tcp_getpeername(&(handle->tcp), addr, &addr_size);
      if (rc == 0) {
        rc = getnameinfo(addr,
                         addr_size,
                         hostbuf,
                         sizeof(hostbuf),
                         portbuf,
                         sizeof(portbuf),
                         NI_NUMERICSERV);
        if (rc == 0) {
          out << " connected to ";
          out << std::string(hostbuf) << ":" << std::string(portbuf);
        }
      } else if (rc == UV_ENOTCONN) {
        out << " (not connected)";
      }
    }
  }
}

// Utility function to format libuv path information.
void ReportPath(uv_handle_t* h, std::ostringstream& out) {
  MallocedBuffer<char> buffer(0);
  int rc = -1;
  size_t size = 0;
  uv_any_handle* handle = reinterpret_cast<uv_any_handle*>(h);
  // First call to get required buffer size.
  switch (h->type) {
    case UV_FS_EVENT: {
      rc = uv_fs_event_getpath(&(handle->fs_event), buffer.data, &size);
      break;
    }
    case UV_FS_POLL: {
      rc = uv_fs_poll_getpath(&(handle->fs_poll), buffer.data, &size);
      break;
    }
    default:
      break;
  }
  if (rc == UV_ENOBUFS) {
    buffer = MallocedBuffer<char>(size);
    switch (h->type) {
      case UV_FS_EVENT: {
        rc = uv_fs_event_getpath(&(handle->fs_event), buffer.data, &size);
        break;
      }
      case UV_FS_POLL: {
        rc = uv_fs_poll_getpath(&(handle->fs_poll), buffer.data, &size);
        break;
      }
      default:
        break;
    }
    if (rc == 0) {
      // buffer is not null terminated.
      std::string name(buffer.data, size);
      out << "filename: " << name;
    }
  }
}

// Utility function to walk libuv handles.
void WalkHandle(uv_handle_t* h, void* arg) {
  const char* type = uv_handle_type_name(h->type);
  std::ostringstream data;
  JSONWriter* writer = static_cast<JSONWriter*>(arg);
  uv_any_handle* handle = reinterpret_cast<uv_any_handle*>(h);

  switch (h->type) {
    case UV_FS_EVENT:
    case UV_FS_POLL:
      ReportPath(h, data);
      break;
    case UV_PROCESS:
      data << "pid: " << handle->process.pid;
      break;
    case UV_TCP:
    case UV_UDP:
      ReportEndpoints(h, data);
      break;
    case UV_TIMER: {
      uint64_t due = handle->timer.timeout;
      uint64_t now = uv_now(handle->timer.loop);
      data << "repeat: " << uv_timer_get_repeat(&(handle->timer));
      if (due > now) {
        data << ", timeout in: " << (due - now) << " ms";
      } else {
        data << ", timeout expired: " << (now - due) << " ms ago";
      }
      break;
    }
    case UV_TTY: {
      int height, width, rc;
      rc = uv_tty_get_winsize(&(handle->tty), &width, &height);
      if (rc == 0) {
        data << "width: " << width << ", height: " << height;
      }
      break;
    }
    case UV_SIGNAL: {
      // SIGWINCH is used by libuv so always appears.
      // See http://docs.libuv.org/en/v1.x/signal.html
      data << "signum: " << handle->signal.signum
#ifndef _WIN32
           << " (" << node::signo_string(handle->signal.signum) << ")"
#endif
           << "";
      break;
    }
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
    if (h->type == UV_TCP || h->type == UV_UDP) {
      data << ", ";
    }
    uv_send_buffer_size(h, &send_size);
    uv_recv_buffer_size(h, &recv_size);
    data << "send buffer size: " << send_size
         << ", recv buffer size: " << recv_size;
  }

  if (h->type == UV_TCP || h->type == UV_NAMED_PIPE || h->type == UV_TTY ||
      h->type == UV_UDP || h->type == UV_POLL) {
    uv_os_fd_t fd_v;
    int rc = uv_fileno(h, &fd_v);
    // uv_os_fd_t is an int on Unix and HANDLE on Windows.
#ifndef _WIN32
    if (rc == 0) {
      switch (fd_v) {
        case 0:
          data << ", stdin";
          break;
        case 1:
          data << ", stdout";
          break;
        case 2:
          data << ", stderr";
          break;
        default:
          data << ", file descriptor: " << static_cast<int>(fd_v);
          break;
      }
    }
#endif
  }

  if (h->type == UV_TCP || h->type == UV_NAMED_PIPE || h->type == UV_TTY) {
    data << ", write queue size: " << handle->stream.write_queue_size;
    data << (uv_is_readable(&handle->stream) ? ", readable" : "")
         << (uv_is_writable(&handle->stream) ? ", writable" : "");
  }

  writer->json_start();
  writer->json_keyvalue("type", type);
  writer->json_keyvalue("is_active", std::to_string(uv_is_active(h)));
  writer->json_keyvalue("is_referenced", std::to_string(uv_has_ref(h)));
  writer->json_keyvalue("address",
                        std::to_string(reinterpret_cast<int64_t>(h)));
  writer->json_keyvalue("details", data.str());
  writer->json_end();
}

static std::string findAndReplace(const std::string& str,
                                  const std::string& old,
                                  const std::string& neu) {
  std::string ret = str;
  size_t pos = 0;
  while ((pos = ret.find(old, pos)) != std::string::npos) {
    ret.replace(pos, old.length(), neu);
    pos += neu.length();
  }
  return ret;
}

std::string EscapeJsonChars(const std::string& str) {
  std::string ret = str;
  ret = findAndReplace(ret, "\\", "\\\\");
  ret = findAndReplace(ret, "\\u", "\\u");
  ret = findAndReplace(ret, "\n", "\\n");
  ret = findAndReplace(ret, "\f", "\\f");
  ret = findAndReplace(ret, "\r", "\\r");
  ret = findAndReplace(ret, "\b", "\\b");
  ret = findAndReplace(ret, "\t", "\\t");
  ret = findAndReplace(ret, "\"", "\\\"");
  return ret;
}

}  // namespace report
