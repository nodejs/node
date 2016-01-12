//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// ES6 super chain tests

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
    {
        name: "Simple base class object construction",
        body: function () {
            class Simple {
                bar() {
                    return 'bar';
                }
                constructor(val) {
                    this.val = val;
                    this.foo = "Simple";
                }
            };

            let result = new Simple('val');

            assert.areEqual("Simple", result.foo, "'this' is valid to use in Simple.constructor");
            assert.areEqual("val", result.val, "Arguments passed to Simple.constructor pass through correctly");
            assert.areEqual("bar", result.bar(), "Result object can call class members");
            assert.isTrue(result instanceof Simple, "new Simple(); uses new.target as 'Simple' itself which creates instanceof Simple class");
        }
    },
    {
        name: "Simple base class object construction which uses a lambda to access this",
        body: function () {
            class Simple {
                constructor(val) {
                    let arrow = () => {
                        this.val = val;
                        this.foo = "Simple";
                    };
                    arrow();
                }
            };

            let result = new Simple('val');

            assert.areEqual("Simple", result.foo, "'this' is valid to use in Simple.constructor");
            assert.areEqual("val", result.val, "Arguments passed to Simple.constructor pass through correctly");
            assert.isTrue(result instanceof Simple, "new Simple(); uses new.target as 'Simple' itself which creates instanceof Simple class");
        }
    },
    {
        name: "Base class constructors return 'this' if they explicitly return a non-object",
        body: function () {
            class ReturnArgumentBaseClass {
                constructor(val) {
                    this.foo = 'ReturnArgumentBaseClass';
                    return val;
                }
            };

            let result = new ReturnArgumentBaseClass(null);
            assert.areEqual("ReturnArgumentBaseClass", result.foo, "new ReturnArgumentBaseClass(null); returns 'this'");
            assert.isTrue(result instanceof ReturnArgumentBaseClass, "new ReturnArgumentBaseClass(null); returns an instance of ReturnArgumentBaseClass");

            result = new ReturnArgumentBaseClass(undefined);
            assert.areEqual("ReturnArgumentBaseClass", result.foo, "new ReturnArgumentBaseClass(undefined); returns 'this'");
            assert.isTrue(result instanceof ReturnArgumentBaseClass, "new ReturnArgumentBaseClass(undefined); returns an instance of ReturnArgumentBaseClass");

            result = new ReturnArgumentBaseClass();
            assert.areEqual("ReturnArgumentBaseClass", result.foo, "new ReturnArgumentBaseClass(); returns 'this'");
            assert.isTrue(result instanceof ReturnArgumentBaseClass, "new ReturnArgumentBaseClass(); returns an instance of ReturnArgumentBaseClass");

            result = new ReturnArgumentBaseClass('string');
            assert.areEqual("ReturnArgumentBaseClass", result.foo, "new ReturnArgumentBaseClass('string'); returns 'this'");
            assert.isTrue(result instanceof ReturnArgumentBaseClass, "new ReturnArgumentBaseClass('string'); returns an instance of ReturnArgumentBaseClass");

            result = new ReturnArgumentBaseClass(5);
            assert.areEqual("ReturnArgumentBaseClass", result.foo, "new ReturnArgumentBaseClass(5); returns 'this'");
            assert.isTrue(result instanceof ReturnArgumentBaseClass, "new ReturnArgumentBaseClass(5); returns an instance of ReturnArgumentBaseClass");
        }
    },
    {
        name: "Base class constructors which return an object override return of 'this'",
        body: function () {
            class ReturnArgumentBaseClass {
                constructor(val) {
                    this.foo = 'ReturnArgumentBaseClass';
                    return val;
                }
            };

            let result = new ReturnArgumentBaseClass({foo:'test'});
            assert.areEqual("test", result.foo, "new ReturnArgumentBaseClass({foo:'test'}); returns {foo:'test'}");
            assert.isFalse(result instanceof ReturnArgumentBaseClass, "new ReturnArgumentBaseClass({foo:'test'}); doesn't return an instance of ReturnArgumentBaseClass");

            result = new ReturnArgumentBaseClass(new Boolean(false));
            assert.areEqual(new Boolean(false), result, "new ReturnArgumentBaseClass(new Boolean(false)); returns new Boolean(false)");
            assert.isTrue(result instanceof Boolean, "new ReturnArgumentBaseClass(new Boolean(false)); returns an instance of Boolean");
        }
    },
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
