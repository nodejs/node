#ifndef SRC_NODE_FILE_UTILS_H_
#define SRC_NODE_FILE_UTILS_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include "uv.h"
#include "v8.h"

namespace node {

// Synchronously writes to a file. If the file exists, it is replaced
// (truncated).
int WriteFileSync(const char* path, uv_buf_t buf);
int WriteFileSync(const char* path, uv_buf_t* bufs, size_t buf_count);
int WriteFileSync(v8::Isolate* isolate,
                  const char* path,
                  v8::Local<v8::String> string);

// Synchronously reads the entire contents of a file.
int ReadFileSync(const char* path, std::string* result);
int ReadFileSync(const char* path, std::vector<uint8_t>* result);

// Legacy interface. TODO(joyeecheung): update the callers to pass path first,
// output parameters second.
int ReadFileSync(std::string* result, const char* path);

// This is currently only used by embedder APIs that take a FILE*.
std::vector<char> ReadFileSync(FILE* fp);

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_FILE_UTILS_H_
