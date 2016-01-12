//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Various ways of calling/loading function with used/unused result.

(function fibonacci(i) {
    Math.atan;
    if (i <= 1) { return 1; }
    return fibonacci(i - 1) + fibonacci(i - 2);

})(1);

(function fibonacci(i) {
    Math.atan(0);
    if (i <= 1) { return 1; }
    return fibonacci(i - 1) + fibonacci(i - 2);

})(2);

(function fibonacci(i) {
    var a = Math.atan;
    if (i <= 1) { return 1; }
    return fibonacci(i - 1) + fibonacci(i - 2);

})(1);

(function fibonacci(i) {
    var a = Math.atan(0);
    if (i <= 1) { return 1; }
    return fibonacci(i - 1) + fibonacci(i - 2);

})(2);

WScript.Echo('pass');