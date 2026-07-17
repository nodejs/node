// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

o = Object("A");
o.x = 1;
Object.seal(o);
o.x = 0.1

o[1] = "b";
assertEquals(undefined, o[1]);
