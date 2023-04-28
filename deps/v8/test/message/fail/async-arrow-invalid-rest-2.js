// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
//

var f = async (a, ...x = 10) => x;
f(1, 2, 3, 4, 5);
