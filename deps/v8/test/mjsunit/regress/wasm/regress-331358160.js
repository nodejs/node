// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

WebAssembly.Suspender = {};
delete WebAssembly.Suspender;
WebAssembly.Suspender = 1;
var x = WebAssembly;
WebAssembly = {};

d8.test.enableJSPI();
d8.test.installConditionalFeatures();
