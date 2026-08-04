// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/gc-js-interop-helpers.js');
export let {struct, array} = CreateWasmObjects();
