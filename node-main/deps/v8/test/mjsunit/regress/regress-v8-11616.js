// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertEquals("x".repeat(3).match(/x(?<=^x{3})/), ["x"]);
assertEquals("x".repeat(4).match(/x(?<=^x{4})/), ["x"]);
assertEquals("x".repeat(7).match(/x(?<=^x{7})/), ["x"]);
assertEquals("x".repeat(17).match(/x(?<=^x{17})/), ["x"]);
