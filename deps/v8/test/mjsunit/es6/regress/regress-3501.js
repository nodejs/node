// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See: http://code.google.com/p/v8/issues/detail?id=3501

"use strict";
let lift = f => (x, k) => k (f (x));
lift(isNaN);
