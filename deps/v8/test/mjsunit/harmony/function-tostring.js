// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-function-tostring

var prefix = "/*before*/";
var suffix = "/*after*/";

function checkStringRepresentation(f, source) {
  assertEquals(typeof f, "function");
  assertEquals(source, f.toString());
}

function testDeclaration(source) {
  // this eval should define a local variable f that is a function
  eval(prefix + source + suffix);
  checkStringRepresentation(f, source);
}
testDeclaration(          "function f(){}");
testDeclaration(          "function*f(){}");
testDeclaration("async     function f(){}");
testDeclaration(          "function/*A*/ f/*B*/(/*C*/a/*D*/,/*E*/b/*G*/)/*H*/{/*I*/}");
testDeclaration(          "function/*A*/*f/*B*/(/*C*/a/*D*/,/*E*/b/*G*/)/*H*/{/*I*/}");
testDeclaration("async/*Z*/function/*A*/ f/*B*/(/*C*/a/*D*/,/*E*/b/*G*/)/*H*/{/*I*/}");
testDeclaration(          "function  \t  f  \n ( \r  a \r\n,\n\r b     )  {'\u2654'}");
testDeclaration(          "function  \t *f  \n ( \r  a \r\n,\n\r b     )     {     }");
testDeclaration(          "function *\t  f  \n ( \r  a \r\n,\n\r b     )     {     }");
testDeclaration("async \t  function      f  \n ( \r  a \r\n,\n\r b     )     {     }");

function testExpression(source) {
  // this eval should return a function
  var f = eval("(" + prefix + source + suffix + ")");
  checkStringRepresentation(f, source);
}
testExpression(          "function  (){}");
testExpression(          "function f(){}");
testExpression(          "function* (){}");
testExpression(          "function*f(){}");
testExpression("async     function  (){}");
testExpression("async     function f(){}");
testExpression(          "function/*A*/  /*B*/(/*C*/a/*D*/,/*E*/b/*G*/)/*H*/{/*I*/}");
testExpression(          "function/*A*/ f/*B*/(/*C*/a/*D*/,/*E*/b/*G*/)/*H*/{/*I*/}");
testExpression(          "function/*A*/* /*B*/(/*C*/a/*D*/,/*E*/b/*G*/)/*H*/{/*I*/}");
testExpression(          "function/*A*/*f/*B*/(/*C*/a/*D*/,/*E*/b/*G*/)/*H*/{/*I*/}");
testExpression("async/*Z*/function/*A*/ f/*B*/(/*C*/a/*D*/,/*E*/b/*G*/)/*H*/{/*I*/}");
testExpression(          "function  \t     \n ( \r  a \r\n,\n\r b     )     {     }");
testExpression(          "function  \t  f  \n ( \r  a \r\n,\n\r b     )     {     }");
testExpression(          "function  \t *   \n ( \r  a \r\n,\n\r b     )     {     }");
testExpression(          "function  \t *f  \n ( \r  a \r\n,\n\r b     )     {     }");
testExpression(          "function *\t     \n ( \r  a \r\n,\n\r b     )     {     }");
testExpression(          "function *\t  f  \n ( \r  a \r\n,\n\r b     )     {     }");
testExpression("async \t  function         \n ( \r  a \r\n,\n\r b     )     {     }");

testExpression(      "(/*A*/ /*B*/ /*C*/ /*D*/ /*E*/ /*F*/)/*G*/=>/*H*/0");
testExpression(            "a/*B*/ /*C*/ /*D*/ /*E*/ /*F*/ /*G*/=>/*H*/{}");
testExpression(      "(/*A*/a/*B*/ /*C*/ /*D*/ /*E*/ /*F*/)/*G*/=>/*H*/0");
testExpression(      "(/*A*/a/*B*/,/*C*/b/*D*/,/*E*/c/*F*/)/*G*/=>/*H*/{}");
testExpression("async (/*A*/ /*B*/ /*C*/ /*D*/ /*E*/ /*F*/)/*G*/=>/*H*/0");
testExpression("async       a/*B*/ /*C*/ /*D*/ /*E*/ /*F*/ /*G*/=>/*H*/{}");
testExpression("async (/*A*/a/*B*/ /*C*/ /*D*/ /*E*/ /*F*/)/*G*/=>/*H*/0");
testExpression("async (/*A*/a/*B*/,/*C*/b/*D*/,/*E*/c/*F*/)/*G*/=>/*H*/{}");

function testSimpleMethod(source) {
  // the source should define a method f

  // object method
  var f = eval("({" + prefix + source + suffix + "}.f)");
  checkStringRepresentation(f, source);

  // nonstatic class method
  var f = eval("new class{" + prefix + source + suffix + "}().f");
  checkStringRepresentation(f, source);

  // static class method
  var f = eval("(class{static" + prefix + source + suffix + "}).f");
  checkStringRepresentation(f, source);
}
testSimpleMethod("f(){}");
testSimpleMethod("*f(){}");
testSimpleMethod("async f(){}");
testSimpleMethod("f \t (){}");
testSimpleMethod("* \tf(){}");
testSimpleMethod("async \t f (){}");

function testAccessorMethod(source, getOrSet) {
  // the source should define a getter or setter method

  // object method
  var f = Object.getOwnPropertyDescriptor(eval("({" + prefix + source + suffix + "})"), "f")[getOrSet];
  checkStringRepresentation(f, source);

  // nonstatic class method
  var f = Object.getOwnPropertyDescriptor(eval("(class{" + prefix + source + suffix + "})").prototype, "f")[getOrSet];

  // static class method
  var f = Object.getOwnPropertyDescriptor(eval("(class{static" + prefix + source + suffix + "})"), "f")[getOrSet];
  checkStringRepresentation(f, source);
}

testAccessorMethod("get f( ){}", "get");
testAccessorMethod("set f(a){}", "set");
testAccessorMethod("get/*A*/f/*B*/(/*C*/ /*D*/)/*E*/{/*F*/}", "get");
testAccessorMethod("set/*A*/f/*B*/(/*C*/a/*D*/)/*E*/{/*F*/}", "set");

const GeneratorFunction = function*(){}.constructor;
const AsyncFunction = async function(){}.constructor;
function testDynamicFunction(...args) {
  var P = args.slice(0, args.length - 1).join(",");
  var bodyText = args.length > 0 ? args[args.length - 1] : "";
  var source = " anonymous(" + P + "\n) {\n" + bodyText + "\n}";
  checkStringRepresentation(         Function(...args),       "function"  + source);
  checkStringRepresentation(GeneratorFunction(...args),       "function*" + source);
  checkStringRepresentation(    AsyncFunction(...args), "async function"  + source);
}
testDynamicFunction();
testDynamicFunction(";");
testDynamicFunction("return");
testDynamicFunction("a", "return a");
testDynamicFunction("a", "b", "return a");
testDynamicFunction("a,   b", "return a");
testDynamicFunction("a,/*A*/b", "return a");
testDynamicFunction("/*A*/a,b", "return a");
testDynamicFunction("a,b", "return a/*A*/");

// Proxies of functions should not throw, but return a NativeFunction.
assertEquals("function () { [native code] }",
             new Proxy(function () { hidden }, {}).toString());
assertEquals("function () { [native code] }",
             new Proxy(() => { hidden }, {}).toString());
assertEquals("function () { [native code] }",
             new Proxy(class {}, {}).toString());
assertEquals("function () { [native code] }",
             new Proxy(function() { hidden }.bind({}), {}).toString());
assertEquals("function () { [native code] }",
             new Proxy(function*() { hidden }, {}).toString());
assertEquals("function () { [native code] }",
             new Proxy(async function() { hidden }, {}).toString());
assertEquals("function () { [native code] }",
             new Proxy(async function*() { hidden }, {}).toString());
assertEquals("function () { [native code] }",
             new Proxy({ method() { hidden } }.method, {}).toString());

// Non-callable proxies still throw.
assertThrows(() => Function.prototype.toString.call(new Proxy({}, {})),
             TypeError);
