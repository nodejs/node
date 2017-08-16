// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Generate a function with a very large closure.
source = "(function() {\n"
for (var i = 0; i < 65000; i++) {
  source += "  var a_" + i + " = 0;\n";
}
source += "  return function() {\n"
for (var i = 0; i < 65000; i++) {
  source += "a_" + i + "++;\n";
}
source += "}})();\n"

eval(source);
