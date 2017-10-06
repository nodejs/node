#ifndef _WIN32

const char* dlopen_pong(void);

const char* dlopen_ping(void) {
  return dlopen_pong();
}

#endif  // _WIN32
