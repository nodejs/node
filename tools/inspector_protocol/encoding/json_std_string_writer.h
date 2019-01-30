// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INSPECTOR_PROTOCOL_ENCODING_JSON_STD_STRING_WRITER_H_
#define INSPECTOR_PROTOCOL_ENCODING_JSON_STD_STRING_WRITER_H_

#include <memory>
#include <string>
#include "json_parser_handler.h"
#include "platform.h"

namespace inspector_protocol {
// Returns a handler object which will write ascii characters to |out|.
// |status->ok()| will be false iff the handler routine HandleError() is called.
// In that case, we'll stop emitting output.
// Except for calling the HandleError routine at any time, the client
// code must call the Handle* methods in an order in which they'd occur
// in valid JSON; otherwise we may crash (the code uses assert).
std::unique_ptr<JSONParserHandler> NewJSONWriter(Platform* platform,
                                                 std::string* out,
                                                 Status* status);
}  // namespace inspector_protocol

#endif  // INSPECTOR_PROTOCOL_ENCODING_JSON_STD_STRING_WRITER_H_
