// Copyright 2019 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "absl/status/status.h"

#include <errno.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <ostream>
#include <string>

#include "absl/base/attributes.h"
#include "absl/base/config.h"
#include "absl/base/internal/raw_logging.h"
#include "absl/base/internal/strerror.h"
#include "absl/base/macros.h"
#include "absl/base/no_destructor.h"
#include "absl/base/nullability.h"
#include "absl/debugging/stacktrace.h"
#include "absl/debugging/symbolize.h"
#include "absl/status/internal/status_internal.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/types/source_location.h"

namespace absl {
ABSL_NAMESPACE_BEGIN

static_assert(
    alignof(status_internal::StatusRep) >= 4,
    "absl::Status assumes it can use the bottom 2 bits of a StatusRep*.");

std::string StatusCodeToString(StatusCode code) {
  return std::string(absl::StatusCodeToStringView(code));
}

absl::string_view StatusCodeToStringView(StatusCode code) {
  switch (code) {
    case StatusCode::kOk:
      return "OK";
    case StatusCode::kCancelled:
      return "CANCELLED";
    case StatusCode::kUnknown:
      return "UNKNOWN";
    case StatusCode::kInvalidArgument:
      return "INVALID_ARGUMENT";
    case StatusCode::kDeadlineExceeded:
      return "DEADLINE_EXCEEDED";
    case StatusCode::kNotFound:
      return "NOT_FOUND";
    case StatusCode::kAlreadyExists:
      return "ALREADY_EXISTS";
    case StatusCode::kPermissionDenied:
      return "PERMISSION_DENIED";
    case StatusCode::kUnauthenticated:
      return "UNAUTHENTICATED";
    case StatusCode::kResourceExhausted:
      return "RESOURCE_EXHAUSTED";
    case StatusCode::kFailedPrecondition:
      return "FAILED_PRECONDITION";
    case StatusCode::kAborted:
      return "ABORTED";
    case StatusCode::kOutOfRange:
      return "OUT_OF_RANGE";
    case StatusCode::kUnimplemented:
      return "UNIMPLEMENTED";
    case StatusCode::kInternal:
      return "INTERNAL";
    case StatusCode::kUnavailable:
      return "UNAVAILABLE";
    case StatusCode::kDataLoss:
      return "DATA_LOSS";
    default:
      return "";
  }
}

std::ostream& operator<<(std::ostream& os, StatusCode code) {
  return os << StatusCodeToString(code);
}

const std::string* absl_nonnull Status::EmptyString() {
  static const absl::NoDestructor<std::string> kEmpty;
  return kEmpty.get();
}

const std::string* absl_nonnull Status::MovedFromString() {
  static const absl::NoDestructor<std::string> kMovedFrom(kMovedFromString);
  return kMovedFrom.get();
}

absl::Status absl::Status::MakeNonOkStatusWithOkCode(
    absl::string_view message) {
  return absl::Status(
      absl::Status::PointerToRep(new absl::status_internal::StatusRep(
          absl::StatusCode::kOk, message, nullptr)));
}

uintptr_t Status::MakeRep(uintptr_t inlined_rep, absl::string_view msg,
                          absl::SourceLocation loc) {
  bool ok = inlined_rep == CodeToInlinedRep(absl::StatusCode::kOk);
  if (ok) return inlined_rep;
  if (msg.empty()
  ) {
    return inlined_rep;
  }
  auto* rep = new status_internal::StatusRep(InlinedRepToCode(inlined_rep), msg,
                                             nullptr);
  if (loc.file_name()[0] != '\0') {
    rep->AddSourceLocation(loc);
  }
  return PointerToRep(rep);
}

uintptr_t Status::AddSourceLocationImpl(uintptr_t rep,
                                        absl::SourceLocation loc) {
  if (IsInlined(rep)) return rep;
  if (loc.file_name()[0] == '\0') return rep;
  status_internal::StatusRep* rep_ptr = PrepareToModify(rep);
  rep_ptr->AddSourceLocation(loc);
  return PointerToRep(rep_ptr);
}

status_internal::StatusRep* absl_nonnull Status::PrepareToModify(
    uintptr_t rep) {
  if (IsInlined(rep)) {
    return new status_internal::StatusRep(InlinedRepToCode(rep),
                                          absl::string_view(), nullptr);
  }
  return RepToPointer(rep)->CloneAndUnref();
}

std::string Status::ToStringSlow(uintptr_t rep, StatusToStringMode mode) {
  if (IsInlined(rep)) {
    return absl::StrCat(absl::StatusCodeToString(InlinedRepToCode(rep)), ": ");
  }
  return RepToPointer(rep)->ToString(mode);
}

std::ostream& operator<<(std::ostream& os, const Status& x) {
  os << x.ToString(StatusToStringMode::kWithEverything);
  return os;
}

namespace status_internal {
// We use an int in the template parameter to shorten mangled names.
template <int error_code>
Status MakeErrorImpl(string_view message, SourceLocation loc) {
  return Status(static_cast<StatusCode>(error_code), message, loc);
}

// Explicit instantiation for all the error codes.
// If we add more error code, we need to add their values on this list.
// Using ints here instead of static_cast<int>(StatusCode::kFoo) makes it easier
// to see that the list is complete.
template Status MakeErrorImpl<0>(string_view, SourceLocation);
template Status MakeErrorImpl<1>(string_view, SourceLocation);
template Status MakeErrorImpl<2>(string_view, SourceLocation);
template Status MakeErrorImpl<3>(string_view, SourceLocation);
template Status MakeErrorImpl<4>(string_view, SourceLocation);
template Status MakeErrorImpl<5>(string_view, SourceLocation);
template Status MakeErrorImpl<6>(string_view, SourceLocation);
template Status MakeErrorImpl<7>(string_view, SourceLocation);
template Status MakeErrorImpl<8>(string_view, SourceLocation);
template Status MakeErrorImpl<9>(string_view, SourceLocation);
template Status MakeErrorImpl<10>(string_view, SourceLocation);
template Status MakeErrorImpl<11>(string_view, SourceLocation);
template Status MakeErrorImpl<12>(string_view, SourceLocation);
template Status MakeErrorImpl<13>(string_view, SourceLocation);
template Status MakeErrorImpl<14>(string_view, SourceLocation);
template Status MakeErrorImpl<15>(string_view, SourceLocation);
template Status MakeErrorImpl<16>(string_view, SourceLocation);
}  // namespace status_internal

bool IsAborted(const Status& status) {
  return status.code() == absl::StatusCode::kAborted;
}

bool IsAlreadyExists(const Status& status) {
  return status.code() == absl::StatusCode::kAlreadyExists;
}

bool IsCancelled(const Status& status) {
  return status.code() == absl::StatusCode::kCancelled;
}

bool IsDataLoss(const Status& status) {
  return status.code() == absl::StatusCode::kDataLoss;
}

bool IsDeadlineExceeded(const Status& status) {
  return status.code() == absl::StatusCode::kDeadlineExceeded;
}

bool IsFailedPrecondition(const Status& status) {
  return status.code() == absl::StatusCode::kFailedPrecondition;
}

bool IsInternal(const Status& status) {
  return status.code() == absl::StatusCode::kInternal;
}

bool IsInvalidArgument(const Status& status) {
  return status.code() == absl::StatusCode::kInvalidArgument;
}

bool IsNotFound(const Status& status) {
  return status.code() == absl::StatusCode::kNotFound;
}

bool IsOutOfRange(const Status& status) {
  return status.code() == absl::StatusCode::kOutOfRange;
}

bool IsPermissionDenied(const Status& status) {
  return status.code() == absl::StatusCode::kPermissionDenied;
}

bool IsResourceExhausted(const Status& status) {
  return status.code() == absl::StatusCode::kResourceExhausted;
}

bool IsUnauthenticated(const Status& status) {
  return status.code() == absl::StatusCode::kUnauthenticated;
}

bool IsUnavailable(const Status& status) {
  return status.code() == absl::StatusCode::kUnavailable;
}

bool IsUnimplemented(const Status& status) {
  return status.code() == absl::StatusCode::kUnimplemented;
}

bool IsUnknown(const Status& status) {
  return status.code() == absl::StatusCode::kUnknown;
}

StatusCode ErrnoToStatusCode(int error_number) {
  switch (error_number) {
    case 0:
      return StatusCode::kOk;
    case EINVAL:        // Invalid argument
    case ENAMETOOLONG:  // Filename too long
    case E2BIG:         // Argument list too long
    case EDESTADDRREQ:  // Destination address required
    case EDOM:          // Mathematics argument out of domain of function
    case EFAULT:        // Bad address
    case EILSEQ:        // Illegal byte sequence
    case ENOPROTOOPT:   // Protocol not available
    case ENOTSOCK:      // Not a socket
    case ENOTTY:        // Inappropriate I/O control operation
    case EPROTOTYPE:    // Protocol wrong type for socket
    case ESPIPE:        // Invalid seek
      return StatusCode::kInvalidArgument;
    case ETIMEDOUT:  // Connection timed out
      return StatusCode::kDeadlineExceeded;
    case ENODEV:  // No such device
    case ENOENT:  // No such file or directory
#ifdef ENOMEDIUM
    case ENOMEDIUM:  // No medium found
#endif
    case ENXIO:  // No such device or address
    case ESRCH:  // No such process
      return StatusCode::kNotFound;
    case EEXIST:         // File exists
    case EADDRNOTAVAIL:  // Address not available
    case EALREADY:       // Connection already in progress
#ifdef ENOTUNIQ
    case ENOTUNIQ:  // Name not unique on network
#endif
      return StatusCode::kAlreadyExists;
    case EPERM:   // Operation not permitted
    case EACCES:  // Permission denied
#ifdef ENOKEY
    case ENOKEY:  // Required key not available
#endif
    case EROFS:  // Read only file system
      return StatusCode::kPermissionDenied;
    case ENOTEMPTY:   // Directory not empty
    case EISDIR:      // Is a directory
    case ENOTDIR:     // Not a directory
    case EADDRINUSE:  // Address already in use
    case EBADF:       // Invalid file descriptor
#ifdef EBADFD
    case EBADFD:  // File descriptor in bad state
#endif
    case EBUSY:    // Device or resource busy
    case ECHILD:   // No child processes
    case EISCONN:  // Socket is connected
#ifdef EISNAM
    case EISNAM:  // Is a named type file
#endif
#ifdef ENOTBLK
    case ENOTBLK:  // Block device required
#endif
    case ENOTCONN:  // The socket is not connected
    case EPIPE:     // Broken pipe
#ifdef ESHUTDOWN
    case ESHUTDOWN:  // Cannot send after transport endpoint shutdown
#endif
    case ETXTBSY:  // Text file busy
#ifdef EUNATCH
    case EUNATCH:  // Protocol driver not attached
#endif
      return StatusCode::kFailedPrecondition;
    case ENOSPC:  // No space left on device
#ifdef EDQUOT
    case EDQUOT:  // Disk quota exceeded
#endif
    case EMFILE:   // Too many open files
    case EMLINK:   // Too many links
    case ENFILE:   // Too many open files in system
    case ENOBUFS:  // No buffer space available
    case ENOMEM:   // Not enough space
#ifdef EUSERS
    case EUSERS:  // Too many users
#endif
      return StatusCode::kResourceExhausted;
#ifdef ECHRNG
    case ECHRNG:  // Channel number out of range
#endif
    case EFBIG:      // File too large
    case EOVERFLOW:  // Value too large to be stored in data type
    case ERANGE:     // Result too large
      return StatusCode::kOutOfRange;
#ifdef ENOPKG
    case ENOPKG:  // Package not installed
#endif
    case ENOSYS:        // Function not implemented
    case ENOTSUP:       // Operation not supported
    case EAFNOSUPPORT:  // Address family not supported
#ifdef EPFNOSUPPORT
    case EPFNOSUPPORT:  // Protocol family not supported
#endif
    case EPROTONOSUPPORT:  // Protocol not supported
#ifdef ESOCKTNOSUPPORT
    case ESOCKTNOSUPPORT:  // Socket type not supported
#endif
    case EXDEV:  // Improper link
      return StatusCode::kUnimplemented;
    case EAGAIN:  // Resource temporarily unavailable
#ifdef ECOMM
    case ECOMM:  // Communication error on send
#endif
    case ECONNREFUSED:  // Connection refused
    case ECONNABORTED:  // Connection aborted
    case ECONNRESET:    // Connection reset
    case EINTR:         // Interrupted function call
#ifdef EHOSTDOWN
    case EHOSTDOWN:  // Host is down
#endif
    case EHOSTUNREACH:  // Host is unreachable
    case ENETDOWN:      // Network is down
    case ENETRESET:     // Connection aborted by network
    case ENETUNREACH:   // Network unreachable
    case ENOLCK:        // No locks available
    case ENOLINK:       // Link has been severed
#ifdef ENONET
    case ENONET:  // Machine is not on the network
#endif
      return StatusCode::kUnavailable;
    case EDEADLK:  // Resource deadlock avoided
#ifdef ESTALE
    case ESTALE:  // Stale file handle
#endif
      return StatusCode::kAborted;
    case ECANCELED:  // Operation cancelled
      return StatusCode::kCancelled;
    default:
      return StatusCode::kUnknown;
  }
}

namespace {
std::string MessageForErrnoToStatus(int error_number,
                                    absl::string_view message) {
  return absl::StrCat(message, ": ",
                      absl::base_internal::StrError(error_number));
}
}  // namespace

Status ErrnoToStatus(int error_number, absl::string_view message,
                     absl::SourceLocation loc) {
  return Status(ErrnoToStatusCode(error_number),
                MessageForErrnoToStatus(error_number, message), loc);
}

const char* absl_nonnull StatusMessageAsCStr(const Status& status) {
  // As an internal implementation detail, we guarantee that if status.message()
  // is non-empty, then the resulting string_view is null terminated.
  auto sv_message = status.message();
  return sv_message.empty() ? "" : sv_message.data();
}

ABSL_NAMESPACE_END
}  // namespace absl
