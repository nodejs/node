// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --gc-interval=164 --stress-compaction

var a = [];
for (var i = 0; i < 10000; i++) { a[i * 100] = 0; }
