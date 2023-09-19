// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertThrows("[({ p: this }), [][0]] = x", SyntaxError);
assertThrows("[...a, [][0]] = []", SyntaxError);
assertThrows("[...o=1,[][0]] = []", SyntaxError);
assertThrows("({x(){},y:[][0]} = {})", SyntaxError);
