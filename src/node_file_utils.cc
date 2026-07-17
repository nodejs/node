#include "node_file_utils.h"

#include <climits>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include "util-inl.h"

#ifdef _WIN32
#include <io.h>  // _S_IREAD _S_IWRITE
#ifndef S_IRUSR
#define S_IRUSR _S_IREAD
#endif  // S_IRUSR
#ifndef S_IWUSR
#define S_IWUSR _S_IWRITE
#endif  // S_IWUSR
#endif

namespace node {

int WriteFileSync(const char* path, uv_buf_t buf) {
  return WriteFileSync(path, &buf, 1);
}

constexpr int64_t kCurrentFileOffset = -1;
int WriteFileSync(const char* path, uv_buf_t* bufs, size_t buf_count) {
  uv_fs_t req;
  int fd = uv_fs_open(nullptr,
                      &req,
                      path,
                      O_WRONLY | O_CREAT | O_TRUNC,
                      S_IWUSR | S_IRUSR,
                      nullptr);
  uv_fs_req_cleanup(&req);
  if (fd < 0) {
    return fd;
  }

  // Handle potential partial writes by looping until all data is written.
  std::vector<uv_buf_t> iovs(bufs, bufs + buf_count);
  size_t idx = 0;

  while (idx < iovs.size()) {
    // Skip empty buffers.
    if (iovs[idx].len == 0) {
      idx++;
      continue;
    }

    uv_fs_write(nullptr,
                &req,
                fd,
                iovs.data() + idx,
                iovs.size() - idx,
                kCurrentFileOffset,
                nullptr);
    if (req.result <= 0) {  // Error during write.
      // UV_EIO should not happen unless the file system is full.
      int err = req.result < 0 ? req.result : UV_EIO;
      uv_fs_req_cleanup(&req);
      uv_fs_close(nullptr, &req, fd, nullptr);
      uv_fs_req_cleanup(&req);
      return err;
    }
    size_t written = req.result;
    uv_fs_req_cleanup(&req);

    // Consume written bytes from buffers.
    while (written > 0 && idx < iovs.size()) {
      if (written >= iovs[idx].len) {
        written -= iovs[idx].len;
        idx++;
      } else {
        iovs[idx].base += written;
        iovs[idx].len -= written;
        written = 0;
      }
    }
  }

  int err = uv_fs_close(nullptr, &req, fd, nullptr);
  uv_fs_req_cleanup(&req);
  return err;
}

int WriteFileSync(v8::Isolate* isolate,
                  const char* path,
                  v8::Local<v8::String> string) {
  node::Utf8Value utf8(isolate, string);
  uv_buf_t buf = uv_buf_init(utf8.out(), utf8.length());
  return WriteFileSync(path, buf);
}

// Default size used if fstat reports a file size of 0 for special files.
static constexpr size_t kDefaultReadSize = 8192;

// The resize_buffer callback is called with the file size after fstat, and must
// return a pointer to a buffer of at least that size. If the file grows during
// reading, resize_buffer may be called again with a larger size; the callback
// must preserve existing content and release old storage if needed.
// After reading completes, resize_buffer may be called with the actual bytes
// read.
template <typename ResizeBuffer>
int ReadFileSyncImpl(const char* path, ResizeBuffer resize_buffer) {
  uv_fs_t req;

  uv_file file = uv_fs_open(nullptr, &req, path, O_RDONLY, 0, nullptr);
  if (req.result < 0) {
    int err = req.result;
    uv_fs_req_cleanup(&req);
    return err;
  }
  uv_fs_req_cleanup(&req);

  // Get the file size first, which should be cheap enough on an already opened
  // files, and saves us from repeated reallocations/reads.
  int err = uv_fs_fstat(nullptr, &req, file, nullptr);
  if (err < 0) {
    uv_fs_req_cleanup(&req);
    uv_fs_close(nullptr, &req, file, nullptr);
    uv_fs_req_cleanup(&req);
    return err;
  }
  // SIZE_MAX is ~18 exabytes on 64-bit and ~4 GB on 32-bit systems.
  // In both cases, the process is unlikely to have that much memory
  // to hold the file content, so we just error with UV_EFBIG.
  if (req.statbuf.st_size > static_cast<uint64_t>(SIZE_MAX)) {
    uv_fs_req_cleanup(&req);
    uv_fs_close(nullptr, &req, file, nullptr);
    uv_fs_req_cleanup(&req);
    return UV_EFBIG;
  }
  size_t size = static_cast<size_t>(req.statbuf.st_size);
  uv_fs_req_cleanup(&req);

  // If the file is reported as 0 bytes for special files, use a default
  // size to start reading.
  if (size == 0) {
    size = kDefaultReadSize;
  }

  char* buffer = resize_buffer(size);
  if (buffer == nullptr) {
    uv_fs_close(nullptr, &req, file, nullptr);
    uv_fs_req_cleanup(&req);
    return UV_ENOMEM;
  }
  size_t total_read = 0;
  while (true) {
    size_t remaining = size - total_read;
    // On Windows, uv_buf_t uses ULONG which may truncate the
    // length for large buffers. Limit the individual read request size to
    // INT_MAX to be safe. The loop will issue multiple reads for larger files.
    if (remaining > INT_MAX) {
      remaining = INT_MAX;
    }
    uv_buf_t buf = uv_buf_init(buffer + total_read, remaining);
    uv_fs_read(
        nullptr, &req, file, &buf, 1 /* nbufs */, kCurrentFileOffset, nullptr);
    ssize_t bytes_read = req.result;
    uv_fs_req_cleanup(&req);
    if (bytes_read < 0) {
      uv_fs_close(nullptr, &req, file, nullptr);
      uv_fs_req_cleanup(&req);
      return bytes_read;
    }
    if (bytes_read == 0) {
      // EOF, stop reading.
      break;
    }
    total_read += bytes_read;
    if (total_read == size) {
      // Buffer is full, the file may have grown during reading.
      // Try increasing the buffer size and reading more.
      if (size == SIZE_MAX) {
        uv_fs_close(nullptr, &req, file, nullptr);
        uv_fs_req_cleanup(&req);
        return UV_EFBIG;
      }
      if (size > SIZE_MAX / 2) {
        size = SIZE_MAX;
      } else {
        size *= 2;
      }
      buffer = resize_buffer(size);
      if (buffer == nullptr) {
        uv_fs_close(nullptr, &req, file, nullptr);
        uv_fs_req_cleanup(&req);
        return UV_ENOMEM;
      }
    }
  }

  int close_err = uv_fs_close(nullptr, &req, file, nullptr);
  uv_fs_req_cleanup(&req);
  if (close_err < 0) {
    return close_err;
  }

  // Truncate the actual size read if necessary.
  if (total_read != size) {
    buffer = resize_buffer(total_read);
    if (buffer == nullptr && total_read != 0) {
      return UV_ENOMEM;
    }
  }
  return 0;
}

int ReadFileSync(const char* path, std::string* result) {
  return ReadFileSyncImpl(path, [result](size_t size) {
    result->resize(size);
    return result->data();
  });
}

// Legacy interface. TODO(joyeecheung): update the callers to pass path first,
// output parameters second.
int ReadFileSync(std::string* result, const char* path) {
  return ReadFileSync(path, result);
}

int ReadFileSync(const char* path, std::vector<uint8_t>* result) {
  return ReadFileSyncImpl(path, [result](size_t size) {
    result->resize(size);
    return reinterpret_cast<char*>(result->data());
  });
}

std::vector<char> ReadFileSync(FILE* fp) {
  CHECK_EQ(ftell(fp), 0);
  int err = fseek(fp, 0, SEEK_END);
  CHECK_EQ(err, 0);
  size_t size = ftell(fp);
  CHECK_NE(size, static_cast<size_t>(-1L));
  err = fseek(fp, 0, SEEK_SET);
  CHECK_EQ(err, 0);

  std::vector<char> contents(size);
  size_t num_read = fread(contents.data(), size, 1, fp);
  CHECK_EQ(num_read, 1);
  return contents;
}

}  // namespace node
