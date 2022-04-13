// Copyright (C) 2017 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: >
    HTML-like comments are not available in module code
    (SingleLineHTMLCloseComment)
esid: sec-html-like-comments
es6id: B1.3
negative:
  phase: early
  type: SyntaxError
flags: [module]
---*/
-->
function f(){}
