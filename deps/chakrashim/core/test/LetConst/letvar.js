//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


function write(x) { WScript.Echo(x); }

// var after let at function scope should be redeclaration error
try {
    eval(
            "(function () {" +
            "   let x = 'let x';" +
            "   var x = 'redeclaration error';" +
            "   write(x);" +
            "})();"
        );
} catch (e) {
    write(e);
}


// var after let in non-function block scope should be valid;
// var declaration should reassing let bound symbol, but should
// also introduce function scoped variable, initialized to undefined
// (Make sure there is no difference whether the let binding is used
// or not before the var declaration)
// (Blue bug 145660)
try {
    eval(
            "(function () {" +
            "   {" +
            "       let x = 'let x';" +
            "       var x = 'var x';" +
            "       write(x);" +
            "   }" +
            "   write(x);" +
            "   {" +
            "       let y = 'let y';" +
            "       write(y);" +
            "       var y = 'var y';" +
            "       write(y);" +
            "   }" +
            "   write(y);" +
            "})();"
        );
} catch (e) {
    write(e);
}

// var before let in non-function block scope should raise a
// Use Berfore Declaration error.
try {
    eval(
            "(function () {" +
            "   {" +
            "       var x = 'var x';" +
            "       let x = 'let x';" +
            "       write(x);" +
            "   }" +
            "   write(x);" +
            "})();"
        );
} catch (e) {
    write(e);
}
