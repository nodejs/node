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

// Like above, but using the default icudt[lb].dat location if icu_data_file is
// not specified.
bool InitializeICUDefaultLocation(const char* exec_path,
                                  const char* icu_data_file);

}  // namespace internal
}  // namespace v8

#endif  // V8_ICU_UTIL_H_
