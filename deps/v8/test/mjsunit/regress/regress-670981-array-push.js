// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var array = [];
array.length = .6e+7;
array.push( );
assertEquals(array.length, .6e+7);
