// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stack-size=300

var re_src = "(?=^)";
for (var i = 0; i < 300; i++) {
  re_src = "(?=(?:" + re_src + "|foo))";
}
var re = new RegExp(re_src);
re.test("sdfdsfsdfdfs");
