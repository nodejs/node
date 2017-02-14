// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MODULE

import {a as x, a as y} from "modules-skip-1.js";
import {b as z, get_a, set_a} from "modules-skip-1.js";

assertEquals(1, get_a());
assertEquals(1, x);
assertEquals(1, y);
assertEquals(1, z);

set_a(2);
assertEquals(2, get_a());
assertEquals(2, x);
assertEquals(2, y);
assertEquals(2, z);

assertThrows(() => x = 3, TypeError);
assertThrows(() => y = 3, TypeError);
assertThrows(() => z = 3, TypeError);

assertEquals(2, get_a());
assertEquals(2, x);
assertEquals(2, y);
assertEquals(2, z);

assertEquals(2, eval('get_a()'));
assertEquals(2, eval('x'));
assertEquals(2, eval('y'));
assertEquals(2, eval('z'));

assertEquals(2, (() => get_a())());
assertEquals(2, (() => x)());
assertEquals(2, (() => y)());
assertEquals(2, (() => z)());
