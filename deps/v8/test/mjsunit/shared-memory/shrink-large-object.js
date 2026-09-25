// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --shared-string-table --expose-gc

let arr = new Array(65535);
gc();
arr[arr.length-1] = 'two';
arr[1] = 'two';
arr[2] = 'two';
arr.length = 2;
gc();
gc();
arr.length = 1;
gc();
gc();
