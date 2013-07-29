// Copyright 2013 the V8 project authors. All rights reserved.
// Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1.  Redistributions of source code must retain the above copyright
//     notice, this list of conditions and the following disclaimer.
// 2.  Redistributions in binary form must reproduce the above copyright
//     notice, this list of conditions and the following disclaimer in the
//     documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
// ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

description("Test behaviour of strict mode");

var globalThisTest;
function testThis() {
    "use strict";
    return this;
}
function testThisDotAccess() {
    "use strict";
    return this.length;
}
function testThisBracketAccess(prop) {
    "use strict";
    return this[prop];
}
function testGlobalAccess() {
    return testThis();
}
function shouldBeSyntaxError(str) {
    shouldThrow(str);
    shouldThrow("(function(){" + str + "})");
}
function testLineContinuation() {
    "use stric\
t";
    return this;
}
function testEscapeSequence() {
    "use\u0020strict";
    return this;
}

shouldBe("testThis.call(null)", "null");
shouldBe("testThis.call(1)", "1");
shouldBe("testThis.call(true)", "true");
shouldBe("testThis.call(false)", "false");
shouldBe("testThis.call(undefined)", "undefined");
shouldBeFalse("testLineContinuation.call(undefined) === undefined");
shouldBeFalse("testEscapeSequence.call(undefined) === undefined");
shouldBe("testThis.call('a string')", "'a string'");
shouldBe("testThisDotAccess.call('a string')", "'a string'.length");
shouldThrow("testThisDotAccess.call(null)");
shouldThrow("testThisDotAccess.call(undefined)");
shouldBeUndefined("testThisDotAccess.call(true)");
shouldBeUndefined("testThisDotAccess.call(false)");
shouldBeUndefined("testThisDotAccess.call(1)");
shouldBe("testThisBracketAccess.call('a string', 'length')", "'a string'.length");
shouldThrow("testThisBracketAccess.call(null, 'length')");
shouldThrow("testThisBracketAccess.call(undefined, 'length')");
shouldBeUndefined("testThisBracketAccess.call(true, 'length')");
shouldBeUndefined("testThisBracketAccess.call(false, 'length')");
shouldBeUndefined("testThisBracketAccess.call(1, 'length')");
shouldBeUndefined("Function('\"use strict\"; return this;')()");
shouldThrow("Function('\"use strict\"; with({});')");


shouldBe("testGlobalAccess()", "undefined");
shouldBe("testThis.call()", "undefined");
shouldBe("testThis.apply()", "undefined");
shouldBe("testThis.call(undefined)", "undefined");
shouldBe("testThis.apply(undefined)", "undefined");

shouldBeSyntaxError("(function eval(){'use strict';})");
shouldBeSyntaxError("(function (eval){'use strict';})");
shouldBeSyntaxError("(function arguments(){'use strict';})");
shouldBeSyntaxError("(function (arguments){'use strict';})");
shouldBeSyntaxError("(function (){'use strict'; var eval;})");
shouldBeSyntaxError("(function (){'use strict'; var arguments;})");
shouldBeSyntaxError("(function (){'use strict'; try{}catch(eval){}})");
shouldBeSyntaxError("(function (){'use strict'; try{}catch(arguments){}})");
shouldBeSyntaxError("(function (a, a){'use strict';})");
shouldBeSyntaxError("(function (a){'use strict'; delete a;})()");
shouldBeSyntaxError("(function (){'use strict'; var a; delete a;})()");
shouldBeSyntaxError("(function (){var a; function f() {'use strict'; delete a;} })()");
shouldBeSyntaxError("(function (){'use strict'; with(1){};})");
shouldThrow("(function (){'use strict'; arguments.callee; })()");
shouldThrow("(function (){'use strict'; arguments.caller; })()");
shouldThrow("(function f(){'use strict'; f.arguments; })()");
shouldThrow("(function f(){'use strict'; f.caller; })()");
shouldThrow("(function f(){'use strict'; f.arguments=5; })()");
shouldThrow("(function f(){'use strict'; f.caller=5; })()");
shouldThrow("(function (arg){'use strict'; arguments.callee; })()");
shouldThrow("(function (arg){'use strict'; arguments.caller; })()");
shouldThrow("(function f(arg){'use strict'; f.arguments; })()");
shouldThrow("(function f(arg){'use strict'; f.caller; })()");
shouldThrow("(function f(arg){'use strict'; f.arguments=5; })()");
shouldThrow("(function f(arg){'use strict'; f.caller=5; })()");

// arguments/caller poisoning should be visible but not throw with 'in' & 'hasOwnProperty'.
shouldBeTrue('"caller" in function(){"use strict"}');
shouldBeTrue('(function(){"use strict";}).hasOwnProperty("caller")');
shouldBeTrue('"arguments" in function(){"use strict"}');
shouldBeTrue('(function(){"use strict";}).hasOwnProperty("arguments")');

shouldBeSyntaxError("'use strict'; (function (){with(1){};})");
shouldBeSyntaxError("'use strict'; (function (){var a; delete a;})");
shouldBeSyntaxError("'use strict'; var a; (function (){ delete a;})");
shouldBeSyntaxError("var a; (function (){ 'use strict'; delete a;})");
shouldBeSyntaxError("'misc directive'; 'use strict'; with({}){}");
shouldThrow("'use strict'; return");
shouldBeSyntaxError("'use strict'; break");
shouldBeSyntaxError("'use strict'; continue");
shouldThrow("'use strict'; for(;;)return");
shouldBeSyntaxError("'use strict'; for(;;)break missingLabel");
shouldBeSyntaxError("'use strict'; for(;;)continue missingLabel");
shouldBeSyntaxError("'use strict'; 007;");
shouldBeSyntaxError("'use strict'; '\\007';");
shouldBeSyntaxError("'\\007'; 'use strict';");

var someDeclaredGlobal;
aDeletableProperty = 'test';

shouldBeSyntaxError("'use strict'; delete aDeletableProperty;");
shouldBeSyntaxError("'use strict'; (function (){ delete someDeclaredGlobal;})");
shouldBeSyntaxError("(function (){ 'use strict'; delete someDeclaredGlobal;})");
shouldBeTrue("'use strict'; if (0) { someGlobal = 'Shouldn\\'t be able to assign this.'; }; true;");
shouldThrow("'use strict'; someGlobal = 'Shouldn\\'t be able to assign this.'; ");
shouldThrow("'use strict'; (function f(){ f = 'shouldn\\'t be able to assign to function expression name'; })()");
shouldThrow("'use strict'; eval('var introducedVariable = \"FAIL: variable introduced into containing scope\";'); introducedVariable");
var objectWithReadonlyProperty = {};
Object.defineProperty(objectWithReadonlyProperty, "prop", {value: "value", writable:false});
shouldThrow("'use strict'; objectWithReadonlyProperty.prop = 'fail'");
shouldThrow("'use strict'; delete objectWithReadonlyProperty.prop");
readonlyPropName = "prop";
shouldThrow("'use strict'; delete objectWithReadonlyProperty[readonlyPropName]");
shouldBeSyntaxError("'use strict'; ++eval");
shouldBeSyntaxError("'use strict'; eval++");
shouldBeSyntaxError("'use strict'; --eval");
shouldBeSyntaxError("'use strict'; eval--");
shouldBeSyntaxError("'use strict'; function f() { ++arguments }");
shouldBeSyntaxError("'use strict'; function f() { arguments++ }");
shouldBeSyntaxError("'use strict'; function f() { --arguments }");
shouldBeSyntaxError("'use strict'; function f() { arguments-- }");
var global = this;
shouldThrow("global.eval('\"use strict\"; if (0) ++arguments; true;')");
shouldBeSyntaxError("'use strict'; ++(1, eval)");
shouldBeSyntaxError("'use strict'; (1, eval)++");
shouldBeSyntaxError("'use strict'; --(1, eval)");
shouldBeSyntaxError("'use strict'; (1, eval)--");
shouldBeSyntaxError("'use strict'; function f() { ++(1, arguments) }");
shouldBeSyntaxError("'use strict'; function f() { (1, arguments)++ }");
shouldBeSyntaxError("'use strict'; function f() { --(1, arguments) }");
shouldBeSyntaxError("'use strict'; function f() { (1, arguments)-- }");
shouldBeSyntaxError("'use strict'; if (0) delete +a.b");
shouldBeSyntaxError("'use strict'; if (0) delete ++a.b");
shouldBeSyntaxError("'use strict'; if (0) delete void a.b");

shouldBeTrue("(function (a){'use strict'; a = false; return a !== arguments[0]; })(true)");
shouldBeTrue("(function (a){'use strict'; arguments[0] = false; return a !== arguments[0]; })(true)");
shouldBeTrue("(function (a){'use strict'; a=false; return arguments; })(true)[0]");
shouldBeTrue("(function (a){'use strict'; arguments[0]=false; return a; })(true)");
shouldBeTrue("(function (a){'use strict'; arguments[0]=true; return arguments; })(false)[0]");
shouldBeTrue("(function (){'use strict';  arguments[0]=true; return arguments; })(false)[0]");
shouldBeTrue("(function (a){'use strict'; arguments[0]=true; a=false; return arguments; })()[0]");
shouldBeTrue("(function (a){'use strict'; arguments[0]=false; a=true; return a; })()");
shouldBeTrue("(function (a){'use strict'; arguments[0]=true; return arguments; })()[0]");
shouldBeTrue("(function (){'use strict';  arguments[0]=true; return arguments; })()[0]");

// Same tests again, this time forcing an activation to be created as well
shouldBeTrue("(function (a){'use strict'; var local; (function (){local;})(); a = false; return a !== arguments[0]; })(true)");
shouldBeTrue("(function (a){'use strict'; var local; (function (){local;})(); arguments[0] = false; return a !== arguments[0]; })(true)");
shouldBeTrue("(function (a){'use strict'; var local; (function (){local;})(); a=false; return arguments; })(true)[0]");
shouldBeTrue("(function (a){'use strict'; var local; (function (){local;})(); arguments[0]=false; return a; })(true)");
shouldBeTrue("(function (a){'use strict'; var local; (function (){local;})(); arguments[0]=true; return arguments; })(false)[0]");
shouldBeTrue("(function (){'use strict';  var local; (function (){local;})(); arguments[0]=true; return arguments; })(false)[0]");
shouldBeTrue("(function (a){'use strict'; var local; (function (){local;})(); arguments[0]=true; a=false; return arguments; })()[0]");
shouldBeTrue("(function (a){'use strict'; var local; (function (){local;})(); arguments[0]=true; return arguments; })()[0]");
shouldBeTrue("(function (a){'use strict'; var local; (function (){local;})(); arguments[0]=false; a=true; return a; })()");
shouldBeTrue("(function (){'use strict';  var local; (function (){local;})(); arguments[0]=true; return arguments; })()[0]");

shouldBeTrue("'use strict'; (function (){var a = true; eval('var a = false'); return a; })()");
shouldBeTrue("(function (){var a = true; eval('\"use strict\"; var a = false'); return a; })()");

shouldBeUndefined("(function f(arg){'use strict'; return Object.getOwnPropertyDescriptor(f, 'arguments').value; })()");
shouldBeUndefined("(function f(arg){'use strict'; return Object.getOwnPropertyDescriptor(f, 'caller').value; })()");
shouldBeUndefined("(function f(arg){'use strict'; return Object.getOwnPropertyDescriptor(arguments, 'callee').value; })()");
shouldBeUndefined("(function f(arg){'use strict'; return Object.getOwnPropertyDescriptor(arguments, 'caller').value; })()");
shouldBeTrue("(function f(arg){'use strict'; var descriptor = Object.getOwnPropertyDescriptor(arguments, 'caller'); return descriptor.get === descriptor.set; })()");
shouldBeTrue("(function f(arg){'use strict'; var descriptor = Object.getOwnPropertyDescriptor(arguments, 'callee'); return descriptor.get === descriptor.set; })()");
shouldBeTrue("(function f(arg){'use strict'; var descriptor = Object.getOwnPropertyDescriptor(f, 'caller'); return descriptor.get === descriptor.set; })()");
shouldBeTrue("(function f(arg){'use strict'; var descriptor = Object.getOwnPropertyDescriptor(f, 'arguments'); return descriptor.get === descriptor.set; })()");
shouldBeTrue("'use strict'; (function f() { for(var i in this); })(); true;")

shouldBeSyntaxError("'use strict'\u033b");
shouldBeSyntaxError("'use strict'5.f");
shouldBeSyntaxError("'use strict';\u033b");
shouldBeSyntaxError("'use strict';5.f");
shouldBeSyntaxError("'use strict';1-(eval=1);");
shouldBeSyntaxError("'use strict';arguments=1;");
shouldBeSyntaxError("'use strict';1-(arguments=1);");
shouldBeSyntaxError("'use strict';var a=(eval=1);");
shouldBeSyntaxError("'use strict';var a=(arguments=1);");

var aGlobal = false;
shouldBeTrue("'use strict'; try { throw 1; } catch (e) { aGlobal = true; }");
aGlobal = false;
shouldBeTrue("'use strict'; (function () { try { throw 1; } catch (e) { aGlobal = true; }})(); aGlobal;");
aGlobal = false;
shouldBeTrue("(function () {'use strict';  try { throw 1; } catch (e) { aGlobal = true; }})(); aGlobal;");
aGlobal = false;
shouldBeTrue("try { throw 1; } catch (e) { aGlobal = true; }");
aGlobal = false;
shouldBeTrue("(function () { try { throw 1; } catch (e) { aGlobal = true; }})(); aGlobal;");
aGlobal = false;
shouldBeTrue("(function () {try { throw 1; } catch (e) { aGlobal = true; }})(); aGlobal;");

// Make sure this doesn't crash!
shouldBe('String(Object.getOwnPropertyDescriptor(function() { "use strict"; }, "caller").get)', "'function () {\\n    [native code]\\n}'");
