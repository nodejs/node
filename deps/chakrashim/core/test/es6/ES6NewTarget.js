//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// ES6 new.target tests

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
    {
        name: "Test new.target parsing path doesn't confuse 'new target'",
        body: function() {
            function target() { return { name: 'something' }; }
            var t = new target; // implicitly 'new target()'

            assert.areEqual('something', t.name, "new target() returned our new object instead of new.target");
        }
    },
    {
        name: "Test new.target in various block scopes'",
        body: function() {
            assert.doesNotThrow(function(){{new.target;}}, "new.target one level block do not throw in a function");
            assert.doesNotThrow(function(){{{new.target;}}}, "new.target two level block do not throw in a function");
            assert.doesNotThrow(function(){with({}) {new.target;}}, "new.target with scope body call does not throw");
            assert.doesNotThrow(function() { function parent(x) { new x();}; function child(){ with(new.target) {toString();}}; parent(child); }, "new.target with scope parameter does not throw");
            assert.doesNotThrow(function(){{if(true){new.target;}}}, "new.target condition block in nested block do not throw in a function");
            assert.doesNotThrow(function(){try { throw Error;} catch(e){new.target;}}, "new.target catch block do not throw in a function");
            assert.doesNotThrow(function(){ var a = b = c = 1; try {} catch([a,b,c]) { new.target;}}, "new.target in CatchParamPattern block do not throw in a function");
            assert.doesNotThrow(function(){ var x = function() {new.target;}; x();}, "new.target in function expression do not throw in a function");
            assert.doesNotThrow(function(){ var o = { "foo" : function () { new.target}}; o.foo();}, "new.target in named function expression do not throw in a function");
        }
    },
    {
        name: "Test new.target parsing path with badly-formed meta-property references",
        body: function() {
            assert.throws(function() { return new['target']; }, TypeError, "Meta-property new.target is not a real property lookup", "Object doesn't support this action");
            assert.throws(function() { return eval('new.'); }, SyntaxError, "Something like 'new.' should fall out of the meta-property parser path", "Syntax error");
            assert.throws(function() { return eval('new.target2'); }, SyntaxError, "No other keywords should produce meta-properties", "Syntax error");
            assert.throws(function() { return eval('new.something'); }, SyntaxError, "No other keywords should produce meta-properties", "Syntax error");
            assert.throws(function() { return eval('new.eval'); }, SyntaxError, "No other keywords should produce meta-properties", "Syntax error");
        }
    },
    {
        name: "There is now a well-known PID for 'target' - ensure it doesn't break",
        body: function() {
            var obj = { target: 'something' };

            assert.areEqual('something', obj.target, "The name 'target' can be used as an identifier");
        }
    },
/*
    // Blocked by 'ReferenceError: '<_lexicalNewTargetSymbol>' is undefined' bug
    {
        name: "new.target is not valid for assignment",
        body: function() {
            assert.throws(function() { eval("new.target = 'something';"); }, ReferenceError, "new.target cannot be a lhs in an assignment expression - this is an early reference error", "Invalid left-hand side in assignment");
            assert.throws(function() { eval("((new.target)) = 'something';"); }, ReferenceError, "new.target cannot be a lhs in an assignment expression - this is an early reference error", "Invalid left-hand side in assignment");
        }
    },
*/
    {
        name: "Simple base class gets new.target correctly",
        body: function() {
            var called = false;

            class SimpleBaseClass {
                constructor() {
                    assert.isTrue(new.target === SimpleBaseClass, "new.target === SimpleBaseClass");

                    called = true;
                }
            }

            var myObj = new SimpleBaseClass();

            assert.isTrue(called, "The constructor was called.");
        }
    },
    {
        name: "Simple derived and base class passes new.target correctly",
        body: function() {
            var called = false;

            class BaseClassForB {
                constructor() {
                    assert.isTrue(new.target === DerivedClassForB, "new.target === DerivedClassForB");

                    called = true;
                }
            }

            class DerivedClassForB extends BaseClassForB {
                constructor() {
                    assert.isTrue(new.target === DerivedClassForB, "new.target === DerivedClassForB");

                    super();
                }
            }

            var myB = new DerivedClassForB();

            assert.isTrue(called, "The super-chain was called.");
        }
    },
    {
        name: "Simple base class with arrow function using new.target correctly",
        body: function() {
            var called = false;

            class SimpleBaseClass {
                constructor() {
                    var arrow = () => {
                        assert.isTrue(new.target === SimpleBaseClass, "new.target === SimpleBaseClass");

                        called = true;
                    }

                    arrow();
                }
            }

            var myObj = new SimpleBaseClass();

            assert.isTrue(called, "The constructor was called.");
        }
    },
    {
        name: "new.target behavior in arrow function inside derived class",
        body: function() {
            let constructed = false;

            class C {
                constructor() {
                    let arrow = () => {
                        assert.isTrue(D === new.target, "Class constructor implicitly invoked via super call has new.target set to derived constructor (also in arrow)");
                        constructed = true;
                    };
                    arrow();
                }
            }

            class D extends C {
                constructor() {
                    let arrow = () => {
                        assert.isTrue(D === new.target, "Class constructor explicitly invoked via new keyword has new.target set to that constructor (also in arrow)");
                    };
                    arrow();
                    super();
                }
            }

            let myD = new D();
            assert.isTrue(constructed, "We actually ran the constructor code");
        }
    },
    {
        name: "new.target behavior in a normal function",
        body: function() {
            function foo() {
                assert.isTrue(undefined === new.target, "Normal function call has new.target set to undefined in the function body");

                return new.target;
            }

            assert.isTrue(undefined === foo(), "Normal function returning new.target returns undefined");
        }
    },
    {
        name: "new.target behavior in a normal function in a new expression",
        body: function() {
            function foo() {
                assert.isTrue(foo === new.target, "Function called as new expression has new.target set to the function in the function body");

                return new.target;
            }

            assert.isTrue(foo === new foo(), "Function called-as-constructor has new.target set to that function");
        }
    },
    {
        name: "new.target behavior in an arrow in a normal function",
        body: function() {
            function foo() {
                let arrow = () => {
                    assert.isTrue(undefined === new.target, "Normal function call has new.target set to undefined in the function body");

                    return new.target;
                };

                return arrow();
            }

            assert.isTrue(undefined === foo(), "Normal function returning new.target returns undefined");
        }
    },
    {
        name: "new.target behaviour in an arrow in a normal function in a new expression",
        body: function() {
            function foo() {
                let arrow = () => {
                    assert.isTrue(foo === new.target, "Function called as new expression has new.target set to the function in the function body");

                    return new.target;
                };

                return arrow();
            }

            assert.isTrue(foo === new foo(), "Function called-as-constructor has new.target set to that function");
        }
    },
    {
        name: "new.target captured from class constructor via arrow",
        body: function() {
            class base {
                constructor() {
                    let arrow = () => {
                        assert.isTrue(derived === new.target, "Function called as new expression has new.target set to the function in the function body");

                        return new.target;
                    };

                    return arrow;
                }
            }
            class derived extends base {
                constructor() {
                    return super();
                }
            }

            let arrow = new derived();

            assert.isTrue(derived === arrow(), "Arrow capturing new.target returns correct value");
        }
    },
    {
        name: "new.target inline constructor case",
        body: function() {
            function foo()
            {
                return new.target;
            }
            function bar()
            {
                return new foo(); //foo will be inlined here
            }
            assert.isTrue(bar() == foo, "Function called as new expression has new.target set to the function in the function body when the constructor is inlined");
        }
    },
    {
        name: "new.target inline  case",
        body: function() {
            function foo()
            {
                return new.target;
            }
            function bar()
            {
                return foo(); //foo will be inlined here
            }
            assert.isTrue(bar() == undefined, "Normal inlined function has new.target set to undefined in the function body");
        }
    },
    {
        name: "new.target generator  case",
        body: function() {
            function *foo()
            {
                yield new.target;
            }
            assert.isTrue((foo()).next().value == undefined, "Generator function has new.target set to undefined in the function body");
        }
    },


];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
