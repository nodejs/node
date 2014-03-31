// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var o = {};
%SetHiddenProperty(o, "test", 1);
// Create non-internalized ""
var empty = "a".substring(1, 1);
assertEquals(undefined, o[empty]);
