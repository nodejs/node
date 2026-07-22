// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc

var x = new Object();
x.__defineGetter__('a', function() { return 7 });
JSON.parse('{"a":2600753951}');
gc();
