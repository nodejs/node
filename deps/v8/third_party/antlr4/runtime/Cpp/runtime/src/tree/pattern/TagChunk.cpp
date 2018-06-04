/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "Exceptions.h"

#include "tree/pattern/TagChunk.h"

using namespace antlr4::tree::pattern;

TagChunk::TagChunk(const std::string& tag) : TagChunk("", tag) {}

TagChunk::TagChunk(const std::string& label, const std::string& tag)
    : _tag(tag), _label(label) {
  if (tag.empty()) {
    throw IllegalArgumentException("tag cannot be null or empty");
  }
}

TagChunk::~TagChunk() {}

std::string TagChunk::getTag() { return _tag; }

std::string TagChunk::getLabel() { return _label; }

std::string TagChunk::toString() {
  if (!_label.empty()) {
    return _label + ":" + _tag;
  }

  return _tag;
}
