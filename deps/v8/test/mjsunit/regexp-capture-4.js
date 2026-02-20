// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertEquals(["xxy", "x"],               /^(x)?\1y$/.exec("xxy"));
assertEquals(null,                       /^(x)?\1y$/.exec("xy"));
assertEquals(["y", undefined],           /^(x)?\1y$/.exec("y"));
assertEquals(null,                       /^(x)?y$/.exec("xxy"));
assertEquals(["xy", "x"],                /^(x)?y$/.exec("xy"));
assertEquals(["y", undefined],           /^(x)?y$/.exec("y"));
assertEquals(["xyzxyz", "xyz", "y"],     /^(x(y)?z){1,2}$/.exec("xyzxyz"));
assertEquals(["xyzxz", "xz", undefined], /^(x(y)?z){1,2}$/.exec("xyzxz"));
assertEquals(["xyz", "xyz", "y"],        /^(x(y)?z){1,2}$/.exec("xyz"));
assertEquals(["xX", "x"],                /(?:(.)\1)?/i.exec("xX"));
assertEquals(["xzxyz", "xyz", "y"],      /^(\2x(y)?z){1,2}$/.exec("xzxyz"));
assertEquals(["xyzxz", "xz", undefined], /^(\2x(y)?z){1,2}$/.exec("xyzxz"));
assertEquals(["xyzxyz", "xyz", "y"],     /^(\2x(y)?z){1,2}$/.exec("xyzxyz"));
