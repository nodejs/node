// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-decorators

assertThrows("class C { acc\\u0065ssor value; }", SyntaxError);
assertThrows("class C { static acc\\u0065ssor value; }", SyntaxError);
