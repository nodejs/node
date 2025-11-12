// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//assertEquals(
//    ["ns", Symbol.toStringTag, Symbol.iterator], Reflect.ownKeys(ns2));
//assertEquals(["ns"], [...ns2]);

export * from "modules-skip-4.mjs";
export * from "modules-skip-5.mjs";
export var zzz = 123;
export {ns2};
import * as ns2 from "modules-namespace2.mjs";
