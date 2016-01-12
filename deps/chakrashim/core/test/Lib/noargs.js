//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

write("escape = " + escape());
write("unescape = " + unescape());
write("eval = " + eval());
write("parseInt = " + parseInt());
write("parseFloat = " + parseFloat());
write("isNaN = " + isNaN());
write("isFinite = " + isFinite());
write("decodeURI = " + decodeURI());
write("encodeURI = " + encodeURI());
write("decodeURIComponent = " + decodeURIComponent());
write("encodeURIComponent = " + encodeURIComponent());
write("Object = " + Object());
write("Function = " + Function());
write("Array = " + Array());
write("String = " + String());
write("Boolean = " + Boolean());
write("Number = " + Number());
//write("Date = " + Date());
write("RegExp = " + RegExp());
write("Error = " + Error());
write("EvalError = " + EvalError());
write("RangeError = " + RangeError());
write("ReferenceError = " + ReferenceError());
write("SyntaxError = " + SyntaxError());
write("TypeError = " + TypeError());
write("URIError = " + URIError());
write("write: " + write);