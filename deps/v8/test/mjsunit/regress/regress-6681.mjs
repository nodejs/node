// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as ns from "./regress-6681.mjs";
export var foo;

assertEquals(false, Reflect.defineProperty(ns, 'foo', {value: 123}));
