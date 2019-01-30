// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INSPECTOR_PROTOCOL_ENCODING_JSON_PARSER_H_
#define INSPECTOR_PROTOCOL_ENCODING_JSON_PARSER_H_

#include <cstdint>
#include <vector>
#include "json_parser_handler.h"
#include "platform.h"
#include "span.h"

namespace inspector_protocol {
// JSON parsing routines.
void ParseJSONChars(const Platform* platform, span<uint8_t> chars,
                    JSONParserHandler* handler);
void ParseJSONChars(const Platform* platform, span<uint16_t> chars,
                    JSONParserHandler* handler);
}  // namespace inspector_protocol

#endif  // INSPECTOR_PROTOCOL_ENCODING_JSON_PARSER_H_
