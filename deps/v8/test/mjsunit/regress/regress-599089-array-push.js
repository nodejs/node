// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


var array = [1.2, 1.2];
array.length = 0;
array.push(undefined);
assertEquals(1, array.length);
assertEquals([undefined], array);
