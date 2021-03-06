// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

assertTrue(%StringIteratorProtector());

const p = ""[Symbol.iterator]().__proto__;
let x = Object.create(p);
x.next = 42;

assertTrue(%StringIteratorProtector());
