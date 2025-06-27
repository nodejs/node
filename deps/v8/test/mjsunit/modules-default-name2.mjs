// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {default as goo} from "modules-skip-default-name2.mjs";
assertEquals(
    {value: "gaga", configurable: true, writable: false, enumerable: false},
    Reflect.getOwnPropertyDescriptor(goo, 'name'));
