// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertThrows("function f() { break }", SyntaxError);
assertThrows("function f() { break a }", SyntaxError);
assertThrows("function f() { continue }", SyntaxError);
assertThrows("function f() { continue a }", SyntaxError);
