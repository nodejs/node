// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var pre = "(?:(?:" + "a".repeat(5460) + "){2}){3}";
var src = pre + "(?:b{100}|c{100})d";
var re = new RegExp(src);

var s = "a".repeat(65536);
assertNull(re.exec(s));
