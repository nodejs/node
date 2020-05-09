// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 2009-2013, International Business Machines
// Corporation and others. All Rights Reserved.
//
// Copyright 2004 and onwards Google Inc.
//
// Author: wilsonh@google.com (Wilson Hsieh)
//

#include "unicode/utypes.h"
#include "unicode/stringpiece.h"
#include "cstring.h"
#include "cmemory.h"

U_NAMESPACE_BEGIN

StringPiece::StringPiece(const char* str)
    : ptr_(str), length_((str == NULL) ? 0 : static_cast<int32_t>(uprv_strlen(str))) { }

StringPiece::StringPiece(const StringPiece& x, int32_t pos) {
  if (pos < 0) {
    pos = 0;
  } else if (pos > x.length_) {
    pos = x.length_;
  }
  ptr_ = x.ptr_ + pos;
  length_ = x.length_ - pos;
}

StringPiece::StringPiece(const StringPiece& x, int32_t pos, int32_t len) {
  if (pos < 0) {
    pos = 0;
  } else if (pos > x.length_) {
    pos = x.length_;
  }
  if (len < 0) {
    len = 0;
  } else if (len > x.length_ - pos) {
    len = x.length_ - pos;
  }
  ptr_ = x.ptr_ + pos;
  length_ = len;
}

void StringPiece::set(const char* str) {
  ptr_ = str;
  if (str != NULL)
    length_ = static_cast<int32_t>(uprv_strlen(str));
  else
    length_ = 0;
}

int32_t StringPiece::find(StringPiece needle, int32_t offset) {
  if (length() == 0 && needle.length() == 0) {
    return 0;
  }
  // TODO: Improve to be better than O(N^2)?
  for (int32_t i = offset; i < length(); i++) {
    int32_t j = 0;
    for (; j < needle.length(); i++, j++) {
      if (data()[i] != needle.data()[j]) {
        i -= j;
        goto outer_end;
      }
    }
    return i - j;
    outer_end: void();
  }
  return -1;
}

int32_t StringPiece::compare(StringPiece other) {
  int32_t i = 0;
  for (; i < length(); i++) {
    if (i == other.length()) {
      // this is longer
      return 1;
    }
    char a = data()[i];
    char b = other.data()[i];
    if (a < b) {
      return -1;
    } else if (a > b) {
      return 1;
    }
  }
  if (i < other.length()) {
    // other is longer
    return -1;
  }
  return 0;
}

U_EXPORT UBool U_EXPORT2
operator==(const StringPiece& x, const StringPiece& y) {
  int32_t len = x.size();
  if (len != y.size()) {
    return false;
  }
  if (len == 0) {
    return true;
  }
  const char* p = x.data();
  const char* p2 = y.data();
  // Test last byte in case strings share large common prefix
  --len;
  if (p[len] != p2[len]) return false;
  // At this point we can, but don't have to, ignore the last byte.
  return uprv_memcmp(p, p2, len) == 0;
}


const int32_t StringPiece::npos = 0x7fffffff;

U_NAMESPACE_END
