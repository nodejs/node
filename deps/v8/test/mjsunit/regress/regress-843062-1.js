// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var sparse_array = [];

sparse_array[100] = 3;
sparse_array[200] = undefined;
sparse_array[300] = 4;
sparse_array[400] = 5;
sparse_array[500] = 6;
sparse_array[600] = 5;
sparse_array[700] = 4;
sparse_array[800] = undefined;
sparse_array[900] = 3
sparse_array[41999] = "filler";

sparse_array.lastIndexOf(3, 99);
