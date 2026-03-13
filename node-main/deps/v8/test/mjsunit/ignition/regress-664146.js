// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var foo_call_count = 0;
function foo() { foo_call_count++; }

// These || and && combinations shouldn't call foo().
(true || foo()) ? 1 : 2;
assertTrue(foo_call_count == 0);
(false && foo()) ? 1 : 2;
assertTrue(foo_call_count == 0);

// These || and && combinations should all call foo().
(foo() || true) ? 1 : 2;
assertTrue(foo_call_count == 1);
(false || foo()) ? 1 : 2;
assertTrue(foo_call_count == 2);
(foo() || false) ? 1 : 2;
assertTrue(foo_call_count == 3);

(true && foo()) ? 1 : 2;
assertTrue(foo_call_count == 4);
(foo() && true) ? 1 : 2;
assertTrue(foo_call_count == 5);
(foo() && false) ? 1 : 2;
assertTrue(foo_call_count == 6);
