// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/parsing/preparse-data.h"
#include "src/base/hashmap.h"
#include "src/base/logging.h"
#include "src/globals.h"
#include "src/objects-inl.h"
#include "src/parsing/parser.h"

namespace v8 {
namespace internal {

PreParseData::FunctionData PreParseData::GetFunctionData(int start) const {
  auto it = functions_.find(start);
  if (it != functions_.end()) {
    return it->second;
  }
  return FunctionData();
}

void PreParseData::AddFunctionData(int start, FunctionData&& data) {
  DCHECK(data.is_valid());
  functions_[start] = std::move(data);
}

void PreParseData::AddFunctionData(int start, const FunctionData& data) {
  DCHECK(data.is_valid());
  functions_[start] = data;
}

size_t PreParseData::size() const { return functions_.size(); }

PreParseData::const_iterator PreParseData::begin() const {
  return functions_.begin();
}

PreParseData::const_iterator PreParseData::end() const {
  return functions_.end();
}

}  // namespace internal
}  // namespace v8.
