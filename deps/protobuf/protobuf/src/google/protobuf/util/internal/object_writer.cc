// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <google/protobuf/util/internal/object_writer.h>

#include <google/protobuf/util/internal/datapiece.h>

namespace google {
namespace protobuf {
namespace util {
namespace converter {

// static
void ObjectWriter::RenderDataPieceTo(const DataPiece& data,
                                     StringPiece name, ObjectWriter* ow) {
  switch (data.type()) {
    case DataPiece::TYPE_INT32: {
      ow->RenderInt32(name, data.ToInt32().ValueOrDie());
      break;
    }
    case DataPiece::TYPE_INT64: {
      ow->RenderInt64(name, data.ToInt64().ValueOrDie());
      break;
    }
    case DataPiece::TYPE_UINT32: {
      ow->RenderUint32(name, data.ToUint32().ValueOrDie());
      break;
    }
    case DataPiece::TYPE_UINT64: {
      ow->RenderUint64(name, data.ToUint64().ValueOrDie());
      break;
    }
    case DataPiece::TYPE_DOUBLE: {
      ow->RenderDouble(name, data.ToDouble().ValueOrDie());
      break;
    }
    case DataPiece::TYPE_FLOAT: {
      ow->RenderFloat(name, data.ToFloat().ValueOrDie());
      break;
    }
    case DataPiece::TYPE_BOOL: {
      ow->RenderBool(name, data.ToBool().ValueOrDie());
      break;
    }
    case DataPiece::TYPE_STRING: {
      ow->RenderString(name, data.ToString().ValueOrDie());
      break;
    }
    case DataPiece::TYPE_BYTES: {
      ow->RenderBytes(name, data.ToBytes().ValueOrDie());
      break;
    }
    case DataPiece::TYPE_NULL: {
      ow->RenderNull(name);
      break;
    }
    default:
      break;
  }
}

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google
