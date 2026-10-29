// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function abc() {
  return 'abc';
}

let str = abc() + 'defghijklmnopqrstuvwxyz😀';
let slice1 = str.substring(0, 26);
%InternalizeString(slice1);
let str2 = %FlattenString(abc() + 'defghijklmnopqrstuvwxyz');
let slice2 = str2.substring(0,20);
%InternalizeString(str2);
assertEquals('ABCDEFGHIJKLMNOPQRST', slice2.toUpperCase());
