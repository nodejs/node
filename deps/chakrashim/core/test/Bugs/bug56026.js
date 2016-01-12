//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Tests that bug 56025 is fixed.
// http://bugcheck/bugs/WindowsBlueBugs/56026

try {
    (function () {
        with ({}) with ({}) with ({}) {
            for (eval("a = 0");
              a < 1;
              function x() {
                  with ({}) with ({}) with ({}) (function y() { new Function })();
                  with ({}) x();
              } ()
            ) {
            }
        }
    })();
}
catch (ex) {
    if (ex.message == "Out of stack space") {
        WScript.Echo("PASSED");
    }
}
