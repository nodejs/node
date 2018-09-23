// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MODULE

let get_x = () => x;

assertEquals(1, x);
assertEquals(1, (() => x)());
assertEquals(1, eval('x'));
assertEquals(1, get_x());

assertThrows(() => x = 666, TypeError);
assertEquals(1, x);
assertEquals(1, (() => x)());
assertEquals(1, eval('x'));
assertEquals(1, get_x());

set_x("foo");
assertEquals("foo", x);
assertEquals("foo", (() => x)());
assertEquals("foo", eval('x'));
assertEquals("foo", get_x());

import {a as x, set_a as set_x} from "modules-skip-1.js"
