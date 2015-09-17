// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --harmony-spread-arrays

var arr = [1, 2, ...[3]];
assertEquals([1, 2, 3], arr);
