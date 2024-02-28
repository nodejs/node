// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-fast-api

const capi = new d8.test.FastCAPI();

assertThrows(() => capi.sum_int64_as_number(0, 1.7976931348623157e+308),
             Error, /out of int64_t range/);
assertThrows(() => capi.sum_uint64_as_number(1.7976931348623157e+308, 0),
             Error, /out of uint64_t range/);
