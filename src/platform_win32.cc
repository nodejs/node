#include "node.h"
#include "platform.h"
#include "platform_win32.h"

#include <v8.h>

#include <errno.h>
#include <sys/param.h> // for MAXPATHLEN
#include <unistd.h> // getpagesize
#include <windows.h>

#include "platform_win32_winsock.cc"

namespace node {

using namespace v8;

static char buf[MAXPATHLEN + 1];
static char *process_title = NULL;


// Does the about the same as perror(), but for windows api functions
void winapi_perror(const char* prefix = NULL) {
  DWORD errorno = GetLastError();
  char *errmsg;

  FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, errorno, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&errmsg, 0, NULL);

  // FormatMessage messages include a newline character

  if (prefix) {
    fprintf(stderr, "%s: %s", prefix, errmsg);
  } else {
    fputs(errmsg, stderr);
  }
}


char** Platform::SetupArgs(int argc, char *argv[]) {
  return argv;
}


void Platform::SetProcessTitle(char *title) {
  // We need to convert _title_ to UTF-16 first, because that's what windows uses internally.
  // It would be more efficient to use the UTF-16 value that we can obtain from v8,
  // but it's not accessible from here.

  // Max title length; according to the specs it should be 64K but in practice it's a little over 30000,
  // but who needs titles that long anyway?
  const int MAX_TITLE_LENGTH = 30001;

  int length;
  WCHAR *title_w;

  // Find out how big the buffer for the wide-char title must be
  length = MultiByteToWideChar(CP_UTF8, 0, title, -1, NULL, 0);
  if (!length) {
    winapi_perror("MultiByteToWideChar");
    return;
  }

  // Convert to wide-char string
  title_w = new WCHAR[length];
  length = MultiByteToWideChar(CP_UTF8, 0, title, -1, title_w, length);
  if (!length) {
    winapi_perror("MultiByteToWideChar");
    delete title_w;
    return;
  };

  // If the title must be truncated insert a \0 terminator there
  if (length > MAX_TITLE_LENGTH) {
    title_w[MAX_TITLE_LENGTH - 1] = *L"\0";
  }

  if (!SetConsoleTitleW(title_w)) {
    winapi_perror("SetConsoleTitleW");
  }

  free(process_title);
  process_title = strdup(title);

  delete title_w;
}


static inline char* _getProcessTitle() {
  WCHAR *title_w;
  char *title;
  int length, length_w;

  length_w = GetConsoleTitleW((WCHAR*)L"\0", sizeof(WCHAR));

  // If length is zero, there may be an error or the title may be empty
  if (!length_w) {
    if (GetLastError()) {
      winapi_perror("GetConsoleTitleW");
      return NULL;
    }
    else {
      // The title is empty, so return empty string
      process_title = strdup("\0");
      return process_title;
    }
  }

  // Room for \0 terminator
  length_w++;

  title_w = new WCHAR[length_w];

  if (!GetConsoleTitleW(title_w, length_w * sizeof(WCHAR))) {
    winapi_perror("GetConsoleTitleW");
    delete title_w;
    return NULL;
  }

  // Find out what the size of the buffer is that we need
  length = WideCharToMultiByte(CP_UTF8, 0, title_w, length_w, NULL, 0, NULL, NULL);
  if (!length) {
    winapi_perror("WideCharToMultiByte");
    delete title_w;
    return NULL;
  }

  title = (char *) malloc(length);
  if (!title) {
    perror("malloc");
    delete title_w;
    return NULL;
  }

  // Do utf16 -> utf8 conversion here
  if (!WideCharToMultiByte(CP_UTF8, 0, title_w, -1, title, length, NULL, NULL)) {
    winapi_perror("WideCharToMultiByte");
    delete title_w;
    free(title);
    return NULL;
  }

  delete title_w;

  return title;
}


const char* Platform::GetProcessTitle(int *len) {
  // If the process_title was never read before nor explicitly set,
  // we must query it with getConsoleTitleW
  if (!process_title) {
    process_title = _getProcessTitle();
  }

  if (process_title) {
    *len = strlen(process_title);
    return process_title;
  } else {
    *len = 0;
    return NULL;
  }
}


int Platform::GetMemory(size_t *rss, size_t *vsize) {
  *rss = 0;
  *vsize = 0;
  return 0;
}


double Platform::GetFreeMemory() {
  return -1;
}

double Platform::GetTotalMemory() {
  return -1;
}


int Platform::GetExecutablePath(char* buffer, size_t* size) {
  *size = 0;
  return -1;
}


int Platform::GetCPUInfo(Local<Array> *cpus) {
  return -1;
}


double Platform::GetUptime() {
  return -1;
}

int Platform::GetLoadAvg(Local<Array> *loads) {
  return -1;
}

} // namespace node
