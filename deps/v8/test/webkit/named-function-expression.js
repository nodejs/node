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

description(
"Tests variable resolution rules for named function expressions."
);

function Call(lambda) { return lambda(); }

debug("anonymous function expression");
shouldBe("var x = (function(a,b){ return a + b; }); x(1,2)", "3");

debug("named function expression");
shouldBe("var x = (function Named(a,b){ return a + b; }); x(2,3)", "5");

debug("eval'd code should be able to access scoped variables");
shouldBe("var z = 6; var x = eval('(function(a,b){ return a + b + z; })'); x(3,4)", "13");

debug("eval'd code + self-check");
shouldBe("var z = 10; var x = eval('(function Named(a,b){ return (!!Named) ? (a + b + z) : -999; })'); x(4,5)", "19");

debug("named function expressions are not saved in the current context");
shouldBe('(function Foo(){ return 1; }); try { Foo(); throw "FuncExpr was stored"; } catch(e) { if(typeof(e)=="string") throw e; } 1', "1");

debug("recursion is possible, though");
shouldBe("var ctr = 3; var x = (function Named(a,b){ if(--ctr) return 2 * Named(a,b); else return a + b; }); x(5,6)", "44");

debug("regression test where kjs regarded an anonymous function declaration (which is illegal) as a FunctionExpr");
shouldBe('var hadError = 0; try { eval("function(){ return 2; };"); } catch(e) { hadError = 1; }; hadError;', "1");

debug("\n-----\n");

function shouldBeTrueWithDescription(x, xDescription)
{
        if (x) {
                debug("PASS: " + xDescription + " should be true and is.");
                return;
        }

        debug("FAIL: " + xDescription + " should be true but isn't.");
}

// Recursion.
shouldBeTrueWithDescription(
        (function closure() { return closure == arguments.callee && !this.closure; })(),
        "(function closure() { return closure == arguments.callee && !this.closure; })()"
);

// Assignment.
shouldBeTrueWithDescription(
        (function closure() { closure = 1; return closure == arguments.callee && !this.closure; })(),
        "(function closure() { closure = 1; return closure == arguments.callee && !this.closure; })()"
);

// Function name vs parameter.
shouldBeTrueWithDescription(
        (function closure(closure) { return closure == 1 && !this.closure; })(1),
        "(function closure(closure) { return closure == 1 && !this.closure; })(1)"
);

// Function name vs var.
shouldBeTrueWithDescription(
        (function closure() { var closure = 1; return closure == 1 && !this.closure; })(),
        "(function closure() { var closure = 1; return closure == 1 && !this.closure; })()"
);

// Function name vs declared function.
shouldBeTrueWithDescription(
        (function closure() { function closure() { }; return closure != arguments.callee && !this.closure; })(),
        "(function closure() { function closure() { }; return closure != arguments.callee && !this.closure; })()"
);

// Resolve before tear-off.
shouldBeTrueWithDescription(
        (function closure() { return (function() { return closure && !this.closure; })(); })(),
        "(function closure() { return (function() { return closure && !this.closure; })(); })()"
);

// Resolve assignment before tear-off.
shouldBeTrueWithDescription(
        (function closure() { return (function() { closure = null; return closure && !this.closure; })(); })(),
        "(function closure() { return (function() { closure = null; return closure && !this.closure; })(); })()"
);

// Resolve after tear-off.
shouldBeTrueWithDescription(
        (function closure() { return (function() { return closure && !this.closure; }); })()(),
        "(function closure() { return (function() { return closure && !this.closure; }); })()()"
);

// Resolve assignment after tear-off.
shouldBeTrueWithDescription(
        (function closure() { return (function() { closure = null; return closure && !this.closure; }); })()(),
        "(function closure() { return (function() { closure = null; return closure && !this.closure; }); })()()"
);

// Eval var shadowing (should overwrite).
shouldBeTrueWithDescription(
        (function closure() { eval("var closure"); return closure == undefined && !this.closure; })(),
        "(function closure() { eval(\"var closure\"); return closure == undefined && !this.closure; })()"
);

// Eval function shadowing (should overwrite).
shouldBeTrueWithDescription(
        (function closure() { eval("function closure() { }"); return closure != arguments.callee && !this.closure; })(),
        "(function closure() { eval(\"function closure() { }\"); return closure != arguments.callee && !this.closure; })()"
);

// Eval shadowing (should overwrite), followed by put (should overwrite).
shouldBeTrueWithDescription(
        (function closure() { eval("var closure;"); closure = 1; return closure == 1 && !this.closure; })(),
        "(function closure() { eval(\"var closure;\"); closure = 1; return closure == 1 && !this.closure; })()"
);

// Eval var shadowing, followed by delete (should not overwrite).
shouldBeTrueWithDescription(
        (function closure() { eval("var closure"); delete closure; return closure == arguments.callee && !this.closure; })(),
        "(function closure() { eval(\"var closure\"); delete closure; return closure == arguments.callee && !this.closure; })()"
);

// Eval function shadowing, followed by delete (should not overwrite).
shouldBeTrueWithDescription(
        (function closure() { eval("function closure() { }"); delete closure; return closure == arguments.callee && !this.closure; })(),
        "(function closure() { eval(\"function closure() { }\"); delete closure; return closure == arguments.callee && !this.closure; })()"
);

// Eval assignment (should not overwrite).
shouldBeTrueWithDescription(
        (function closure() { eval("closure = 1;"); return closure == arguments.callee && !this.closure; })(),
        "(function closure() { eval(\"closure = 1;\"); return closure == arguments.callee && !this.closure; })()"
);
