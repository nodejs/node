// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Used for building without embedded data.

#include <cstdint>

#include "src/base/macros.h"

extern "C" const uint8_t* v8_Default_embedded_blob_;
extern "C" uint32_t v8_Default_embedded_blob_size_;

const uint8_t* v8_Default_embedded_blob_ = nullptr;
uint32_t v8_Default_embedded_blob_size_ = 0;

#ifdef V8_MULTI_SNAPSHOTS
extern "C" const uint8_t* v8_Trusted_embedded_blob_;
extern "C" uint32_t v8_Trusted_embedded_blob_size_;

const uint8_t* v8_Trusted_embedded_blob_ = nullptr;
uint32_t v8_Trusted_embedded_blob_size_ = 0;
#endif
