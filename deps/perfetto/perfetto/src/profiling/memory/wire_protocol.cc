/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/profiling/memory/wire_protocol.h"

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/unix_socket.h"
#include "perfetto/ext/base/utils.h"

#include <sys/socket.h>
#include <sys/types.h>

namespace perfetto {
namespace profiling {

namespace {
template <typename T>
bool ViewAndAdvance(char** ptr, T** out, const char* end) {
  if (end - sizeof(T) < *ptr)
    return false;
  *out = reinterpret_cast<T*>(*ptr);
  *ptr += sizeof(T);
  return true;
}

// We need this to prevent crashes due to FORTIFY_SOURCE.
void UnsafeMemcpy(char* dest, const char* src, size_t n)
    __attribute__((no_sanitize("address", "hwaddress"))) {
  for (size_t i = 0; i < n; ++i) {
    dest[i] = src[i];
  }
}
}  // namespace

bool SendWireMessage(SharedRingBuffer* shmem, const WireMessage& msg) {
  uint64_t total_size;
  struct iovec iovecs[3] = {};
  // TODO(fmayer): Maye pack these two.
  iovecs[0].iov_base = const_cast<RecordType*>(&msg.record_type);
  iovecs[0].iov_len = sizeof(msg.record_type);
  if (msg.alloc_header) {
    PERFETTO_DCHECK(msg.record_type == RecordType::Malloc);
    iovecs[1].iov_base = msg.alloc_header;
    iovecs[1].iov_len = sizeof(*msg.alloc_header);
  } else if (msg.free_header) {
    PERFETTO_DCHECK(msg.record_type == RecordType::Free);
    iovecs[1].iov_base = msg.free_header;
    iovecs[1].iov_len = sizeof(*msg.free_header);
  } else {
    PERFETTO_DFATAL_OR_ELOG("Neither alloc_header nor free_header set.");
    errno = EINVAL;
    return false;
  }

  iovecs[2].iov_base = msg.payload;
  iovecs[2].iov_len = msg.payload_size;

  struct msghdr hdr = {};
  hdr.msg_iov = iovecs;
  if (msg.payload) {
    hdr.msg_iovlen = base::ArraySize(iovecs);
    total_size = iovecs[0].iov_len + iovecs[1].iov_len + iovecs[2].iov_len;
  } else {
    // If we are not sending payload, just ignore that iovec.
    hdr.msg_iovlen = base::ArraySize(iovecs) - 1;
    total_size = iovecs[0].iov_len + iovecs[1].iov_len;
  }

  SharedRingBuffer::Buffer buf;
  {
    ScopedSpinlock lock = shmem->AcquireLock(ScopedSpinlock::Mode::Try);
    if (!lock.locked()) {
      PERFETTO_DLOG("Failed to acquire spinlock.");
      errno = EAGAIN;
      return false;
    }
    buf = shmem->BeginWrite(lock, static_cast<size_t>(total_size));
  }
  if (!buf) {
    PERFETTO_DLOG("Buffer overflow.");
    shmem->EndWrite(std::move(buf));
    errno = EAGAIN;
    return false;
  }

  size_t offset = 0;
  for (size_t i = 0; i < hdr.msg_iovlen; ++i) {
    UnsafeMemcpy(reinterpret_cast<char*>(buf.data + offset),
                 reinterpret_cast<const char*>(hdr.msg_iov[i].iov_base),
                 hdr.msg_iov[i].iov_len);
    offset += hdr.msg_iov[i].iov_len;
  }
  shmem->EndWrite(std::move(buf));
  return true;
}

bool ReceiveWireMessage(char* buf, size_t size, WireMessage* out) {
  RecordType* record_type;
  char* end = buf + size;
  if (!ViewAndAdvance<RecordType>(&buf, &record_type, end)) {
    PERFETTO_DFATAL_OR_ELOG("Cannot read record type.");
    return false;
  }

  out->payload = nullptr;
  out->payload_size = 0;
  out->record_type = *record_type;

  if (*record_type == RecordType::Malloc) {
    if (!ViewAndAdvance<AllocMetadata>(&buf, &out->alloc_header, end)) {
      PERFETTO_DFATAL_OR_ELOG("Cannot read alloc header.");
      return false;
    }
    out->payload = buf;
    if (buf > end) {
      PERFETTO_DFATAL_OR_ELOG("Receive buffer overflowed");
      return false;
    }
    out->payload_size = static_cast<size_t>(end - buf);
  } else if (*record_type == RecordType::Free) {
    if (!ViewAndAdvance<FreeBatch>(&buf, &out->free_header, end)) {
      PERFETTO_DFATAL_OR_ELOG("Cannot read free header.");
      return false;
    }
  } else {
    PERFETTO_DFATAL_OR_ELOG("Invalid record type.");
    return false;
  }
  return true;
}

}  // namespace profiling
}  // namespace perfetto
