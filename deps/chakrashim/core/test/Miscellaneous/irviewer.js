//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function foo() {
  return 1;
}

function blah(a,b) {
  return (a+b)*2+42;
}


// parseIR
WScript.Echo("--- parseIR ---");
var x = parseIR("function bar() {return 42;}");  // <<< CALL parseIR


// functionList
WScript.Echo("--- functionList ---");
var fnlist = functionList();  // <<< CALL functionList


// rejitFunction
WScript.Echo("--- rejitFunction ---");

var fn, fnSrcInfo, fnId;
fn = fnlist[0];
fnSrcInfo = fn.utf8SrcInfoPtr;
fnId = fn.funcId;

rejitFunction(fnSrcInfo, fnId);  // <<< CALL rejitFunction
