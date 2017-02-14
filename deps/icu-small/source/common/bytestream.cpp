// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 2009-2011, International Business Machines
// Corporation and others. All Rights Reserved.
//
// Copyright 2007 Google Inc. All Rights Reserved.
// Author: sanjay@google.com (Sanjay Ghemawat)

#include "unicode/utypes.h"
#include "unicode/bytestream.h"
#include "cmemory.h"

U_NAMESPACE_BEGIN

ByteSink::~ByteSink() {}

char* ByteSink::GetAppendBuffer(int32_t min_capacity,
                                int32_t /*desired_capacity_hint*/,
                                char* scratch, int32_t scratch_capacity,
                                int32_t* result_capacity) {
  if (min_capacity < 1 || scratch_capacity < min_capacity) {
    *result_capacity = 0;
    return NULL;
  }
  *result_capacity = scratch_capacity;
  return scratch;
}

void ByteSink::Flush() {}

CheckedArrayByteSink::CheckedArrayByteSink(char* outbuf, int32_t capacity)
    : outbuf_(outbuf), capacity_(capacity < 0 ? 0 : capacity),
      size_(0), appended_(0), overflowed_(FALSE) {
}

CheckedArrayByteSink::~CheckedArrayByteSink() {}

CheckedArrayByteSink& CheckedArrayByteSink::Reset() {
  size_ = appended_ = 0;
  overflowed_ = FALSE;
  return *this;
}

void CheckedArrayByteSink::Append(const char* bytes, int32_t n) {
  if (n <= 0) {
    return;
  }
  appended_ += n;
  int32_t available = capacity_ - size_;
  if (n > available) {
    n = available;
    overflowed_ = TRUE;
  }
  if (n > 0 && bytes != (outbuf_ + size_)) {
    uprv_memcpy(outbuf_ + size_, bytes, n);
  }
  size_ += n;
}

char* CheckedArrayByteSink::GetAppendBuffer(int32_t min_capacity,
                                            int32_t /*desired_capacity_hint*/,
                                            char* scratch,
                                            int32_t scratch_capacity,
                                            int32_t* result_capacity) {
  if (min_capacity < 1 || scratch_capacity < min_capacity) {
    *result_capacity = 0;
    return NULL;
  }
  int32_t available = capacity_ - size_;
  if (available >= min_capacity) {
    *result_capacity = available;
    return outbuf_ + size_;
  } else {
    *result_capacity = scratch_capacity;
    return scratch;
  }
}

U_NAMESPACE_END
