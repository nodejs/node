// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbo-fast-api-calls

const fast_c_api = new d8.test.FastCAPI();
const fast_obj = Object.create(fast_c_api);
assertThrows(() => fast_obj.supports_fp_params);
