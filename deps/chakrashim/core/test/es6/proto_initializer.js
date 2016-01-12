//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

/// <reference path="protolib.js" />
if (this.WScript && this.WScript.LoadScriptFile) {
    WScript.LoadScriptFile("protolib.js");
}

// Test __proto__ (object literal) initializer in a new engine:
//      Run "enabled" function, disable__proto__, then run "disabled" (or by default "enabled" again).
function test_init(enabled, /*optional*/disabled) {
    var eng = make_engine();

    eng.run(enabled);
    eng.disable__proto__();
    eng.run(disabled || enabled);
}

var tests = [
    {
        name: "init to an object",
        body: function () {
            test_init(
                function () { // enabled: [[prototype]]
                    var p = { p: 123 };
                    var o = { __proto__: p };

                    assert.areEqual(p, Object.getPrototypeOf(o));
                    assert.isTrue(!o.hasOwnProperty("__proto__"));
                    assert.areEqual(123, o.p);
                    assert.areEqual(p, Object.getPrototypeOf(o));
                });
        }
    },

    {
        name: "init to null",
        body: function () {
            test_init(
                function () { // enabled: [[prototype]]
                    var o = { __proto__: null };

                    assert.areEqual(null, Object.getPrototypeOf(o));
                    assert.isFalse({}.hasOwnProperty.apply(o, ["__proto__"]));
                    assert.areEqual(undefined, o.__proto__); // o's [[prototype]] is null, so doesn't have a __proto__ property
                });
        }
    },

    {
        name: "init to neither object nor null",
        body: function () {
            test_init(
                function () { // enabled: throw
                    function test(value) {
                        var o = { __proto__: value };

                        assert.areEqual(Object.prototype, Object.getPrototypeOf(o));
                        assert.isFalse(o.hasOwnProperty("__proto__"));
                    }

                    [undefined, 0, 123, Infinity, true, false, "string value"].forEach(function (value) {
                        test(value);
                    });
                });
        }
    },

    {
        name: "init to accessor",
        body: function () {
            test_init(
                function () { // same for enabled/disabled: local property
                    var o = {
                        get __proto__() { return "proto"; },
                        set __proto__(value) { this.__proto__value = value; }
                    };

                    assert.areEqual(Object.prototype, Object.getPrototypeOf(o));
                    assert.isTrue(o.hasOwnProperty("__proto__"));
                    assert.areEqual("proto", o.__proto__);
                    o.__proto__ = "a value";
                    assert.areEqual("a value", o.__proto__value);
                });
        }
    },

    {
        name: "verify no incorrectly shared type",
        body: function () {
            function foo(p) {
                return {
                    a: 100,
                    __proto__: p,
                };
            }

            // If we incorrectly shared Type, we'll have wrong [[prototype]].
            var o1 = foo({ b: 1 });
            var o2 = foo({ b: 2 });
            var o3 = foo({ b: 3 });

            assert.areEqual(1, o1.b);
            assert.areEqual(2, o2.b);
            assert.areEqual(3, o3.b);
        }
    },

    {
        name: "verify not accidentally enables it for function parameters",
        body: function () {
            function foo(a, b, __proto__) {
                var o = arguments;

                assert.areEqual(Object.prototype, Object.getPrototypeOf(o));
                assert.areEqual(Object.prototype, o.__proto__);
                assert.isTrue(!o.hasOwnProperty("__proto__"));
                assert.areEqual(1, o[0]);
                assert.areEqual(3, o[2].a);
                assert.areEqual(4, __proto__.b);
            }
            foo(1, 2, { a: 3, b: 4, c: 5 });
        }
    },

    {
        name: "verify not accidentally enables it for JSON",
        body: function () {
            var o = JSON.parse('{ "a": 1, "b": 2, "__proto__": {"c": 3, "d": 4} }');

            assert.areEqual(Object.prototype, Object.getPrototypeOf(o));
            assert.isTrue(o.hasOwnProperty("__proto__"));
            assert.areEqual(3, o.__proto__.c);
        }
    },

    {
        name: "Verify not accidentally share code with global InitFld",
        body: function () {
            // Check if we accidentally changed global's [[prototype]] to a function when declaring a global
            // function with name __proto__ (see bottom of this file). If yes, we'd have "length" property.
            assert.areEqual(undefined, this.length);
        }
    },

    {
        name: "Run the same initializer with __proto__ enabled, run it again with __proto__ disabled",
        body: function () {
            var eng = make_engine();

            // inject global g_p and foo into eng
            eng.eval("var g_p = { p: 123 }");
            eng.eval("var foo = " + function() {
                return { a: 0, __proto__: g_p, b: 1 };
            });

            var test = function () { // enabled: [[prototype]]
                var o = foo();
                assert.areEqual(g_p, Object.getPrototypeOf(o));
                assert.areEqual("a,b", Object.keys(o).toString());
                assert.areEqual(123, o.p);
            };

            eng.run(test);
            eng.disable__proto__();
            eng.run(test);
        }
    },

    {
        name: "Enumeration order should be unaffected",
        body: function () {
            test_init(
                function () {
                    var o = {
                        a: 100,
                        __proto__: new Number(200),
                        b: 300,
                    };
                    assert.areEqual("a,b", Object.keys(o).toString());
                });

            test_init(
                function () { // enabled: [[prototype]]
                    var o = {
                        a: 100,
                        __proto__: { c: "p0", d: "p1" },
                        b: 300,
                    };

                    var names = [];
                    for (var name in o) {
                        names.push(name);
                    }

                    assert.areEqual("a,b", Object.keys(o).toString());
                    assert.areEqual("a,b,c,d", names.toString());
                });
        }
    },

    {
        name: "Verify bytecode serialization",
        body: function () { // Test in current engine to use switch -ForceSerialized
            var o = {
                a: 100,
                __proto__: { c: "p0", d: "p1" },
                b: 300,
            };

            // Serialized bytecode should correctly mark if initializer has__proto__
            assert.areEqual("a,b", Object.keys(o).toString());
        }
    },

];
testRunner.run(tests);

// Used by: Verify not accidentally share code with global InitFld
function __proto__() { }
