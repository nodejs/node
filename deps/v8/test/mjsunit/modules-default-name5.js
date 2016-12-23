// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MODULE

import {default as goo} from "modules-skip-default-name5.js";
assertEquals(
    {value: "Gaga", configurable: true, writable: false, enumerable: false},
    Reflect.getOwnPropertyDescriptor(goo, 'name'));
