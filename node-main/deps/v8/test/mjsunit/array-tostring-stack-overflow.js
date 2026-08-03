// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stack-size=100

var ary=[0];
for(var i=1; i<10000; i++)
  ary=[ary, i];

assertThrows(() => ary.toString(),
             RangeError, 'Maximum call stack size exceeded');
