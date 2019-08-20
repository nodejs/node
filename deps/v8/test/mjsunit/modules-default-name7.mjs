// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {default as goo} from "modules-skip-default-name7.mjs";
let descr = Reflect.getOwnPropertyDescriptor(goo, 'name');
assertEquals(descr,
    {value: descr.value, configurable: true, writable: true, enumerable: false});
assertEquals("yo", descr.value());
