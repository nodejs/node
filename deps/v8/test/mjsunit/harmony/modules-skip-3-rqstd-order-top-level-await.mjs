// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Function('return this;')().test262.push('3_dir_a');
let m = import('modules-skip-3-rqstd-order.mjs');
await m;
Function('return this;')().test262.push('3_dir_b');
