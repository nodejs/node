// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags:

let PI = 0;
for (PI = -2205; PI < 24; PI++)
    [, PI, ...[-3, ...[42, -4294967301, ...[- 8993, ...Array()]]]];
