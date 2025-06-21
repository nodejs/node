// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function f() {
 return ((0%0)&1) + (1>>>(0%0));
}

f();
