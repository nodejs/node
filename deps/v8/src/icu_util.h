// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef V8_ICU_UTIL_H_
#define V8_ICU_UTIL_H_

namespace v8 {

namespace internal {

// Call this function to load ICU's data tables for the current process.  This
// function should be called before ICU is used.
bool InitializeICU(const char* icu_data_file);

} }  // namespace v8::internal

#endif  // V8_ICU_UTIL_H_
