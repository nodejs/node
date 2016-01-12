// Copyright Microsoft. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and / or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#include "jsrtutils.h"
#include <memory>
namespace jsrt {

using std::unique_ptr;
JsErrorCode StringConvert::GetCharLength(const wchar_t *str,
                                         const size_t length,
                                         const int code,
                                         __out size_t *utf8Length) {
  *utf8Length = 0;

  // Compute the size of the required buffer
  DWORD size = WideCharToMultiByte(code,
                                   0,
                                   str,
                                   static_cast<int>(length),
                                   NULL,
                                   0,
                                   NULL,
                                   NULL);
  if (size == 0) {
    return JsErrorFatal;
  }

  *utf8Length = size;

  return JsNoError;
}

JsErrorCode StringConvert::InternalToChar(const wchar_t *str,
                                          const size_t length,
                                          const int code,
                                          char* buffer,
                                          size_t bufferSize,
                                          __out size_t *bytesWritten,
                                          __out size_t *charsWritten) {
  if (bytesWritten != nullptr) {
    *bytesWritten = 0;
  }

  // Compute the size of the required buffer
  size_t size;
  auto getCharLengthResult = GetCharLength(str, length, code, &size);

  if (getCharLengthResult != JsNoError) {
    return getCharLengthResult;
  }

  if (bufferSize == -1) {
    bufferSize = size;
  }

  size_t strLengthToWrite = length;
  // buffer size is insuffecient
  if (bufferSize < size) {
    // in the horrible situation in which the buffer size isn't sufficient we
    // mimic the behavior of v8 and find the maximal number of characters that
    // we can fit in the given buffer in order to do so, the only thing that we
    // can do is try and remove characters from the string and call
    // WideCharToMultiByte each time unfortenately, WideCharToMultiByte will
    // fail if the buffer size is insufficient, so this is the only way to do
    // this
    size_t requiredBufferSize = size;
    strLengthToWrite = length - 1;
    for (; strLengthToWrite > 0; strLengthToWrite--) {
      auto getCharLengthResult =
        GetCharLength(str, strLengthToWrite, code, &requiredBufferSize);

      if (getCharLengthResult != JsNoError) {
        return getCharLengthResult;
      }

      if (requiredBufferSize <= bufferSize) {
        break;
      }
    }
  }

  // if nothing could be written, just leave
  if (strLengthToWrite == 0) {
    if (bytesWritten != nullptr) {
      *bytesWritten = 0;
    }

    if (charsWritten != nullptr) {
      *charsWritten = 0;
    }

    return JsNoError;
  }

  // Do the actual conversion
  DWORD result = WideCharToMultiByte(code,
                                     0,
                                     str,
                                     static_cast<int>(strLengthToWrite),
                                     buffer,
                                     static_cast<int>(bufferSize),
                                     NULL,
                                     NULL);

  if (result == 0) {
    DWORD conversionError = GetLastError();
    if (conversionError != ERROR_INSUFFICIENT_BUFFER) {
      // This should never happen.
      return JsErrorFatal;
    }
  }

  if (bytesWritten != nullptr) {
    *bytesWritten = result;
  }

  if (charsWritten != nullptr) {
    *charsWritten = strLengthToWrite;
  }

  return JsNoError;
}

JsErrorCode StringConvert::InternalToWChar(const char *str,
                                           const size_t length,
                                           const int code,
                                           wchar_t* buffer,
                                           size_t size,
                                           __out size_t *charsWritten) {
  //
  // Special case of empty input string
  //
  if (str == nullptr || length == 0) {
    return JsNoError;
  }

  //
  // Get length (in wchar_t's) of resulting UTF-16 string
  //
  const int utf16Length = ::MultiByteToWideChar(
    code,                      // convert the given code
    0,                         // flags
    str,                       // source string
    static_cast<int>(length),  // length (in chars) of source string
    NULL,                      // unused - no conversion done in this step
    0);           // request size of destination buffer, in wchar_t's

  if (utf16Length == 0) {
    // Error
    return JsErrorFatal;
  }

  if (size == -1) {
    size = utf16Length;
  }

  //
  // Do the conversion from code to UTF-16
  //
  *charsWritten = ::MultiByteToWideChar(
    code,                       // convert from code
    0,                          // validation was done in previous call,
                                // so speed up things with default flags
    str,                        // source string
    static_cast<int>(length),   // length (in chars) of source string
    buffer,                     // destination buffer
    static_cast<int>(size));    // size of destination buffer, in wchar_t's

  if (*charsWritten == 0) {
    // Error
    return JsErrorFatal;
  }

  //
  // Return resulting UTF-16 string
  //
  return JsNoError;
}

}  // namespace jsrt
