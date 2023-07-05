// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Used for building without embedded data.

#include <cstdint>

extern "C" const uint8_t v8_Default_embedded_blob_code_[];
extern "C" uint32_t v8_Default_embedded_blob_code_size_;
extern "C" const uint8_t v8_Default_embedded_blob_data_[];
extern "C" uint32_t v8_Default_embedded_blob_data_size_;

const uint8_t v8_Default_embedded_blob_code_[1] = {0};
uint32_t v8_Default_embedded_blob_code_size_ = 0;
const uint8_t v8_Default_embedded_blob_data_[1] = {0};
uint32_t v8_Default_embedded_blob_data_size_ = 0;
