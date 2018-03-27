#include "node_report.h"

#ifdef __APPLE__
#include <crt_externs.h>  // _NSGetArgv() and _NSGetArgc()
#endif
#ifdef __sun
#include <procfs.h>  // psinfo_t structure
#endif
#ifdef _AIX
#include <sys/procfs.h>  // psinfo_t structure
#endif

namespace nodereport {

/*******************************************************************************
 * Function to process node-report config: selection of trigger events.
 ******************************************************************************/
unsigned int ProcessNodeReportEvents(const char* args) {
  // Parse the supplied event types
  unsigned int event_flags = 0;
  const char* cursor = args;
  while (*cursor != '\0') {
    if (!strncmp(cursor, "exception", sizeof("exception") - 1)) {
      event_flags |= NR_EXCEPTION;
      cursor += sizeof("exception") - 1;
    } else if (!strncmp(cursor, "fatalerror", sizeof("fatalerror") - 1)) {
       event_flags |= NR_FATALERROR;
       cursor += sizeof("fatalerror") - 1;
    } else if (!strncmp(cursor, "signal", sizeof("signal") - 1)) {
      event_flags |= NR_SIGNAL;
      cursor += sizeof("signal") - 1;
    } else if (!strncmp(cursor, "apicall", sizeof("apicall") - 1)) {
      event_flags |= NR_APICALL;
      cursor += sizeof("apicall") - 1;
    } else {
      std::cerr << "Unrecognised argument for node-report events option: " << cursor << "\n";
      return 0;
    }
    if (*cursor == '+') {
      cursor++;  // Hop over the '+' separator
    }
  }
  return event_flags;
}

/*******************************************************************************
 * Function to process node-report config: selection of trigger signal.
 ******************************************************************************/
unsigned int ProcessNodeReportSignal(const char* args) {
#ifdef _WIN32
  return 0; // no-op on Windows
#else
  if (strlen(args) == 0) {
    std::cerr << "Missing argument for node-report signal option\n";
  } else {
    // Parse the supplied switch
    if (!strncmp(args, "SIGUSR2", sizeof("SIGUSR2") - 1)) {
      return SIGUSR2;
    } else if (!strncmp(args, "SIGQUIT", sizeof("SIGQUIT") - 1)) {
      return SIGQUIT;
    } else {
     std::cerr << "Unrecognised argument for node-report signal option: "<< args << "\n";
    }
  }
  return SIGUSR2;  // Default signal is SIGUSR2
#endif
}

/*******************************************************************************
 * Function to process node-report config: specification of report file name.
 ******************************************************************************/
void ProcessNodeReportFileName(const char* args) {
  if (strlen(args) == 0) {
    std::cerr << "Missing argument for node-report filename option\n";
    return;
  }
  if (strlen(args) > NR_MAXNAME) {
    std::cerr << "Supplied node-report filename too long (max " << NR_MAXNAME << " characters)\n";
    return;
  }
  snprintf(report_filename, sizeof(report_filename), "%s", args);
}

/*******************************************************************************
 * Function to process node-report config: specification of report directory.
 ******************************************************************************/
void ProcessNodeReportDirectory(const char* args) {
  if (strlen(args) == 0) {
    std::cerr << "Missing argument for node-report directory option\n";
    return;
  }
  if (strlen(args) > NR_MAXPATH) {
    std::cerr << "Supplied node-report directory path too long (max " << NR_MAXPATH << " characters)\n";
    return;
  }
  snprintf(report_directory, sizeof(report_directory), "%s", args);
}

/*******************************************************************************
 * Function to process node-report config: verbose mode switch.
 ******************************************************************************/
unsigned int ProcessNodeReportVerboseSwitch(const char* args) {
  if (strlen(args) == 0) {
    std::cerr << "Missing argument for node-report verbose switch option\n";
    return 0;
  }
  // Parse the supplied switch
  if (!strncmp(args, "yes", sizeof("yes") - 1) || !strncmp(args, "true", sizeof("true") - 1)) {
    return 1;
  } else if (!strncmp(args, "no", sizeof("no") - 1) || !strncmp(args, "false", sizeof("false") - 1)) {
    return 0;
  } else {
    std::cerr << "Unrecognised argument for node-report verbose switch option: " << args << "\n";
  }
  return 0;  // Default is verbose mode off
}

/*******************************************************************************
 * Function to save the node and subcomponent version strings. This is called
 * during node-report module initialisation.
 *******************************************************************************/
void SetVersionString(Isolate* isolate) {
  // Catch anything thrown and gracefully return
  Nan::TryCatch trycatch;
  version_string = UNKNOWN_NODEVERSION_STRING;

  // Retrieve the process object
  v8::Local<v8::String> process_prop;
  if (!Nan::New<v8::String>("process").ToLocal(&process_prop)) return;
  v8::Local<v8::Object> global_obj = isolate->GetCurrentContext()->Global();
  v8::Local<v8::Value> process_value;
  if (!Nan::Get(global_obj, process_prop).ToLocal(&process_value)) return;
  if (!process_value->IsObject()) return;
  v8::Local<v8::Object> process_obj = process_value.As<v8::Object>();

  // Get process.version
  v8::Local<v8::String> version_prop;
  if (!Nan::New<v8::String>("version").ToLocal(&version_prop)) return;
  v8::Local<v8::Value> version;
  if (!Nan::Get(process_obj, version_prop).ToLocal(&version)) return;

  // e.g. Node.js version: v6.9.1
  if (version->IsString()) {
    Nan::Utf8String node_version(version);
    version_string = "Node.js version: ";
    version_string += *node_version;
    version_string += "\n";
  }

  // Get process.versions
  v8::Local<v8::String> versions_prop;
  if (!Nan::New<v8::String>("versions").ToLocal(&versions_prop)) return;
  v8::Local<v8::Value> versions_value;
  if (!Nan::Get(process_obj, versions_prop).ToLocal(&versions_value)) return;
  if (!versions_value->IsObject()) return;
  v8::Local<v8::Object> versions_obj = versions_value.As<v8::Object>();

  // Get component names and versions from process.versions
  v8::Local<v8::Array> components;
  if (!Nan::GetOwnPropertyNames(versions_obj).ToLocal(&components)) return;
  v8::Local<v8::Object> components_obj = components.As<v8::Object>();
  std::string comp_versions = "(";
  size_t wrap = 0;
  uint32_t total_components = (*components)->Length();
  for (uint32_t i = 0; i < total_components; i++) {
    v8::Local<v8::Value> name_value;
    if (!Nan::Get(components_obj, i).ToLocal(&name_value)) continue;
    v8::Local<v8::Value> version_value;
    if (!Nan::Get(versions_obj, name_value).ToLocal(&version_value)) continue;

    Nan::Utf8String component_name(name_value);
    Nan::Utf8String component_version(version_value);
    if (*component_name == nullptr || *component_version == nullptr) continue;

    if (!strcmp("node", *component_name)) {
      // Put the Node.js version on the first line, if we didn't already have it
      if (version_string == UNKNOWN_NODEVERSION_STRING) {
        version_string = "Node.js version: v";
        version_string += *component_version;
        version_string += "\n";
      }
    } else {
      // Other component versions follow, comma separated, wrapped at 80 characters
      std::string comp_version_string = *component_name;
      comp_version_string += ": ";
      comp_version_string += *component_version;
      if (wrap == 0) {
        wrap = comp_version_string.length();
      } else {
        wrap += comp_version_string.length() + 2;  // includes separator
        if (wrap > 80) {
          comp_versions += ",\n ";
          wrap = comp_version_string.length();
        } else {
          comp_versions += ", ";
        }
      }
      comp_versions += comp_version_string;
    }
  }
  version_string += comp_versions + ")\n";
}

/*******************************************************************************
 * Function to save the node-report module load time value. This is called
 * during node-report module initialisation.
 *******************************************************************************/
void SetLoadTime() {
#ifdef _WIN32
  GetLocalTime(&loadtime_tm_struct);
#else  // UNIX, OSX
  struct timeval time_val;
  gettimeofday(&time_val, nullptr);
  localtime_r(&time_val.tv_sec, &loadtime_tm_struct);
#endif
  time(&load_time);
}

/*******************************************************************************
 * Function to save the process command line. This is called during node-report
 * module initialisation.
 *******************************************************************************/
void SetCommandLine() {
#ifdef __linux__
  // Read the command line from /proc/self/cmdline
  char buf[64];
  FILE* cmdline_fd = fopen("/proc/self/cmdline", "r");
  if (cmdline_fd == nullptr) {
    return;
  }
  commandline_string = "";
  int bytesread = fread(buf, 1, sizeof(buf), cmdline_fd);
  while (bytesread > 0) {
    for (int i = 0; i < bytesread; i++) {
      // Arguments are null separated.
      if (buf[i] == '\0') {
        commandline_string += " ";
      } else {
        commandline_string += buf[i];
      }
    }
    bytesread = fread(buf, 1, sizeof(buf), cmdline_fd);
  }
  fclose(cmdline_fd);
#elif __APPLE__
  char **argv = *_NSGetArgv();
  int argc = *_NSGetArgc();

  commandline_string = "";
  std::string separator = "";
  for (int i = 0; i < argc; i++) {
    commandline_string += separator + argv[i];
    separator = " ";
  }
#elif defined(_AIX) || defined(__sun)
  // Read the command line from /proc/self/cmdline
  char procbuf[64];
  snprintf(procbuf, sizeof(procbuf), "/proc/%d/psinfo", getpid());
  FILE* psinfo_fd = fopen(procbuf, "r");
  if (psinfo_fd == nullptr) {
    return;
  }
  psinfo_t info;
  int bytesread = fread(&info, 1, sizeof(psinfo_t), psinfo_fd);
  fclose(psinfo_fd);
  if (bytesread == sizeof(psinfo_t)) {
    commandline_string = "";
    std::string separator = "";
#ifdef _AIX
    char **argv = *((char ***) info.pr_argv);
#else
    char **argv = ((char **) info.pr_argv);
#endif
    for (uint32_t i = 0; i < info.pr_argc && argv[i] != nullptr; i++) {
      commandline_string += separator + argv[i];
      separator = " ";
    }
  }
#elif _WIN32
  commandline_string = GetCommandLine();
#endif
}

/*******************************************************************************
 * Utility function to format libuv socket information.
 *******************************************************************************/
void reportEndpoints(uv_handle_t* h, std::ostringstream& out) {
  struct sockaddr_storage addr_storage;
  struct sockaddr* addr = (sockaddr*)&addr_storage;
  char hostbuf[NI_MAXHOST];
  char portbuf[NI_MAXSERV];
  uv_any_handle* handle = (uv_any_handle*)h;
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
    default: break;
  }
  if (rc == 0) {
    // getnameinfo will format host and port and handle IPv4/IPv6.
    rc = getnameinfo(addr, addr_size, hostbuf, sizeof(hostbuf), portbuf,
                     sizeof(portbuf), NI_NUMERICSERV);
    if (rc == 0) {
      out << std::string(hostbuf) << ":" << std::string(portbuf);
    }

    if (h->type == UV_TCP) {
      // Get the remote end of the connection.
      rc = uv_tcp_getpeername(&(handle->tcp), addr, &addr_size);
      if (rc == 0) {
        rc = getnameinfo(addr, addr_size, hostbuf, sizeof(hostbuf), portbuf,
                         sizeof(portbuf), NI_NUMERICSERV);
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

/*******************************************************************************
 * Utility function to format libuv path information.
 *******************************************************************************/
void reportPath(uv_handle_t* h, std::ostringstream& out) {
  char *buffer = nullptr;
  int rc = -1;
  size_t size = 0;
  uv_any_handle* handle = (uv_any_handle*)h;
  // First call to get required buffer size.
  switch (h->type) {
    case UV_FS_EVENT: {
      rc = uv_fs_event_getpath(&(handle->fs_event), buffer, &size);
      break;
    }
    case UV_FS_POLL: {
      rc = uv_fs_poll_getpath(&(handle->fs_poll), buffer, &size);
      break;
    }
    default: break;
  }
  if (rc == UV_ENOBUFS) {
    buffer = static_cast<char *>(malloc(size));
    switch (h->type) {
      case UV_FS_EVENT: {
        rc = uv_fs_event_getpath(&(handle->fs_event), buffer, &size);
        break;
      }
      case UV_FS_POLL: {
        rc = uv_fs_poll_getpath(&(handle->fs_poll), buffer, &size);
        break;
      }
      default: break;
    }
    if (rc == 0) {
      // buffer is not null terminated.
      std::string name(buffer, size);
      out << "filename: " << name;
    }
    free(buffer);
  }
}

/*******************************************************************************
 * Utility function to walk libuv handles.
 *******************************************************************************/
void walkHandle(uv_handle_t* h, void* arg) {
  std::string type;
  std::ostringstream data;
  std::ostream* out = reinterpret_cast<std::ostream*>(arg);
  uv_any_handle* handle = (uv_any_handle*)h;

  // List all the types so we get a compile warning if we've missed one,
  // (using default: supresses the compiler warning).
  switch (h->type) {
    case UV_UNKNOWN_HANDLE: type = "unknown"; break;
    case UV_ASYNC: type = "async"; break;
    case UV_CHECK: type = "check"; break;
    case UV_FS_EVENT: {
      type = "fs_event";
      reportPath(h, data);
      break;
    }
    case UV_FS_POLL: {
      type = "fs_poll";
      reportPath(h, data);
      break;
    }
    case UV_HANDLE: type = "handle"; break;
    case UV_IDLE: type = "idle"; break;
    case UV_NAMED_PIPE: type = "pipe"; break;
    case UV_POLL: type = "poll"; break;
    case UV_PREPARE: type = "prepare"; break;
    case UV_PROCESS: {
      type = "process";
      data << "pid: " << handle->process.pid;
      break;
    }
    case UV_STREAM: type = "stream"; break;
    case UV_TCP: {
      type = "tcp";
      reportEndpoints(h, data);
      break;
    }
    case UV_TIMER: {
      // TODO timeout/due is not actually public however it is present
      // in all current versions of libuv. Once uv_timer_get_timeout is
      // in a supported level of libuv we should test for it with dlsym
      // and use it instead, in case timeout moves in the future.
#ifdef _WIN32
      uint64_t due = handle->timer.due;
#else
      uint64_t due = handle->timer.timeout;
#endif
      uint64_t now = uv_now(handle->timer.loop);
      type = "timer";
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
      type = "tty";
      rc = uv_tty_get_winsize(&(handle->tty), &width, &height);
      if (rc == 0) {
        data << "width: " << width << ", height: " << height;
      }
      break;
    }
    case UV_UDP: {
      type = "udp";
      reportEndpoints(h, data);
      break;
    }
    case UV_SIGNAL: {
      // SIGWINCH is used by libuv so always appears.
      // See http://docs.libuv.org/en/v1.x/signal.html
      type = "signal";
      data << "signum: " << handle->signal.signum
      // node::signo_string() is not exported by Node.js on Windows.
#ifndef _WIN32
           << " (" << node::signo_string(handle->signal.signum) << ")"
#endif
           ;
      break;
    }
    case UV_FILE: type = "file"; break;
    // We shouldn't see "max" type
    case UV_HANDLE_TYPE_MAX : type = "max"; break;
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
    uv_os_fd_t* fd = &fd_v;
    int rc  = uv_fileno(h, fd);
    // uv_os_fd_t is an int on Unix and HANDLE on Windows.
#ifndef _WIN32
    if (rc == 0) {
      switch (fd_v) {
      case 0:
        data << ", stdin"; break;
      case 1:
        data << ", stdout"; break;
      case 2:
        data << ", stderr"; break;
      default:
        data << ", file descriptor: " << static_cast<int>(fd_v);
        break;
      }
    }
#endif
  }

  if (h->type == UV_TCP || h->type == UV_NAMED_PIPE || h->type == UV_TTY) {

    data << ", write queue size: "
         << handle->stream.write_queue_size;
    data << (uv_is_readable(&handle->stream) ? ", readable" : "")
         << (uv_is_writable(&handle->stream) ? ", writable": "");

  }

  *out << std::left << "[" << (uv_has_ref(h) ? 'R' : '-')
       << (uv_is_active(h) ? 'A' : '-') << "]   " << std::setw(10) << type
       << std::internal << std::setw(2 + 2 * sizeof(void*));
  char prev_fill = out->fill('0');
  *out << static_cast<void*>(h) << std::left;
  out->fill(prev_fill);
  *out << "  " << std::left << data.str() << std::endl;
}

/*******************************************************************************
 * Utility function to print out integer values with commas for readability.
 ******************************************************************************/
void WriteInteger(std::ostream& out, size_t value) {
  int thousandsStack[8];  // Sufficient for max 64-bit number
  int stackTop = 0;
  int i;
  char buf[64];
  size_t workingValue = value;

  do {
    thousandsStack[stackTop++] = workingValue % 1000;
    workingValue /= 1000;
  } while (workingValue != 0);

  for (i = stackTop-1; i >= 0; i--) {
    if (i == (stackTop-1)) {
      out << thousandsStack[i];
    } else {
      snprintf(buf, sizeof(buf), "%03u", thousandsStack[i]);
      out << buf;
    }
    if (i > 0) {
      out << ",";
    }
  }
}

}  // namespace nodereport
