// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const initial = function () {};
const descriptor = {"element": "externref", "initial": 3};
new WebAssembly.Table(descriptor, initial);
