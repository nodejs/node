// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbo-fast-api-calls --expose-fast-api --allow-natives-syntax --turbofan

const fast_c_api = new d8.test.FastCAPI();
assertThrows(() => {fast_c_api.add_all_sequence()});
