// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-externalize-string --parallel-compile-tasks-for-lazy

const source = "function f() {}";
externalizeString(source);
eval(source);

// Keep main thread alive to allow background thread to run
const start = Date.now();
while(Date.now() - start < 1000) {}
