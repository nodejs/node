// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original repro. A DCHECK used to fire.
(function* (name = (eval(foo), foo, prototype)) { });

// Simpler repro.
(function (name = (foo, bar, baz) ) { });

// A test which uses the value of the n-ary operation.
(function (param = (0, 1, 2)) { assertEquals(2, param); })();
