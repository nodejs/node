#ifndef _WIN32

const char* dlopen_pong(void);

__attribute__((visibility("default")))
const char* dlopen_ping(void) {
  return dlopen_pong();
}

#endif  // _WIN32
