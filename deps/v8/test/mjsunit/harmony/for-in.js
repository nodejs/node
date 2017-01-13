// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-for-in

assertThrows("for (var x = 0 in {});", SyntaxError);
assertThrows("for (const x = 0 in {});", SyntaxError);
assertThrows("for (let x = 0 in {});", SyntaxError);
