// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let a = [,,,,,,,,,,,,,,,,,,,,,,,11,12,13,14,15,16,17,18,19];
%NormalizeElements(a);
let b = a.slice(19);
assertEquals(11, b[4]);
