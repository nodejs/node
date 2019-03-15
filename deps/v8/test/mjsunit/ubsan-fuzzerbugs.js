// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// crbug.com/923466
__v_5 = [ -1073741825, -2147483648];
__v_5.sort();

// crbug.com/923642
new RegExp("(abcd){2148473648,}", "");

// crbug.com/923626
new Date(2146399200000).toString();
new Date(2146940400000).toString();
new Date(2147481600000).toString();
new Date(2148022800000).toString();

// crbug.com/927212
assertThrows(() => (2n).toString(-2147483657), RangeError);
