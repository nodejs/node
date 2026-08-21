// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertThrows(`
{
    function a() {}
}

{
    // Duplicate lexical declarations are only allowed if they are both sloppy
    // block functions (see bug 4693). In this case the sloppy block function
    // conflicts with the lexical variable declaration, causing a syntax error.
    let a;
    function a() {};
}
`, SyntaxError)
