// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Make sure printing different element kinds doesn't crash.

var array;
var obj = {};

array = [];
%DebugPrint(array);

// FAST_SMI_ELEMENTS
array = [1, 2, 3];
%DebugPrint(array);

// FAST_HOLEY_SMI_ELEMENTS
array[10] = 100;
array[11] = 100;
%DebugPrint(array);

// FAST_ELEMENTS
array = [1, obj, obj];
%DebugPrint(array);

// FAST_HOLEY_ELEMENTS
array[100] = obj;
array[101] = obj;
%DebugPrint(array);

// FAST_DOUBLE_ELEMENTS
array = [1.1, 2.2, 3.3, 3.3, 3.3, NaN];
%DebugPrint(array);
array.push(NaN);
array.push(NaN);
%DebugPrint(array);

// FAST_HOLEY_DOUBLE_ELEMENTS
array[100] = 1.2;
array[101] = 1.2;
%DebugPrint(array);

// DICTIONARY_ELEMENTS
%NormalizeElements(array);
%DebugPrint(array);
