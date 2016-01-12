//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

/// <reference path="protolib.js" />
if (this.WScript && this.WScript.LoadScriptFile) {
    WScript.LoadScriptFile("protolib.js");
}

var tests = [
    {
        name: "Change Object.prototype.__proto__ value",
        body: function () {
            // This considered no-op: Object.prototype.__proto__ = null
            verify_disable("Object.prototype.__proto__ = null", KEEP_ENABLED);

            // Set to these primitives will throw and make no change
            [undefined, 0, 123, -12.3, NaN, Infinity, true, false, "str"].forEach(
                function (newValue) {
                    Object.prototype.__proto__ = newValue;
                    verify__proto__enabled();
                });

            // Set to any objects will throw and make no change
            [new Boolean(), new Number(12), new String("string object"), {}, [], Object.prototype, Math.sin, assert.throws].forEach(
                function (newValue) {
                    assert.throws__proto__Cyclic(function () {
                        Object.prototype.__proto__ = newValue;
                    });
                    verify__proto__enabled();
                });
        }
    },

    {
        name: "seal/freeze Object.prototype",
        body: function () {
            ["Object.seal(Object.prototype)", "Object.freeze(Object.prototype)"].forEach(function (expr) {
                verify_disable(expr, KEEP_ENABLED); // These no longer disables __proto__
            });
        }
    },

    {
        name: "delete Object.prototype.__proto__",
        body: function () {
            var eng = make_engine();

            var desc = eng.Object.getOwnPropertyDescriptor(eng.Object.prototype, "__proto__");
            eng.verify_disable("delete Object.prototype.__proto__");

            // redefine such a property brings __proto__ back
            eng.Object.defineProperty(eng.Object.prototype, "__proto__", desc);
            eng.verify__proto__enabled();
        }
    },

    {
        name: "DefineOwnProperty with missing/different attribute set",
        body: function () {
            function getDescString(desc) {
                var set = [];
                for (var name in desc) {
                    set.push(name + ": " + desc[name]);
                }
                return '{' + set.join(", ") + '}';
            }

            function testDesc(desc, keepEnabled) {
                var expr = 'Object.defineProperty(Object.prototype, "__proto__", ' + getDescString(desc) + ')';

                var eng = make_engine();
                eng.__verify_disable(expr, keepEnabled);

                // Verify we indeed have those attr set successfully
                var otherDesc = eng.Object.getOwnPropertyDescriptor(eng.Object.prototype, "__proto__");
                for (var name in desc) {
                    assert.areEqual("" + desc[name], "" + otherDesc[name], name);
                }
            }

            var ATTR_NAMES = ["enumerable", "configurable"];
            var DEF_DESC = { enumerable: false, configurable: true };

            // Test any attr missing
            for (var n = 0; n < 3; n++) {
                var desc = {};
                for (var i = 0; i < 2; i++) {
                    if (n & (1 << i)) {
                        var name = ATTR_NAMES[i];
                        desc[name] = DEF_DESC[name];
                    }
                }

                testDesc(desc, KEEP_ENABLED); // Now these don't disable __proto__
            }

            // Test any attr diff
            ATTR_NAMES.forEach(function (attr) {
                var desc = { enumerable: false, configurable: true };
                desc[attr] = !desc[attr];

                testDesc(desc, KEEP_ENABLED); // Now these don't disable __proto__
            });

            testDesc({ enumerable: false, configurable: true }, KEEP_ENABLED);

            // But will be disabled if we change to value, or change setter
            testDesc({ value: 234, writable: true, enumerable: false, configurable: true });
            testDesc({ set: function () { return "custom setter" }, enumerable: false, configurable: true });
        }
    },

    {
        name: "Change Object.prototype.__proto__ getter or setter",
        body: function () {
            make_engine().run(function () {
                var oldDesc = Object.getOwnPropertyDescriptor(Object.prototype, "__proto__");

                var p = { p: 12 };
                var p2 = { p2: 23 };
                var o = { o: 34 };

                assert.areEqual(Object.prototype, o.__proto__);
                o.__proto__ = p;
                assert.areEqual(p, Object.getPrototypeOf(o));

                // Replace the setter
                Object.defineProperty(Object.prototype, "__proto__", { set: function () { } });
                assert.areEqual(p, o.__proto__);
                o.__proto__ = p2; // This does not work
                assert.areEqual(p, o.__proto__);
                Object.setPrototypeOf(o, p2); // But this works
                assert.areEqual(p2, o.__proto__);

                // Replace the getter
                Object.defineProperty(Object.prototype, "__proto__", { get: function () { return 123; } });
                assert.areEqual(123, o.__proto__);
                assert.areEqual(p2, Object.getPrototypeOf(o));
                Object.setPrototypeOf(o, p);
                assert.areEqual(123, o.__proto__);
                assert.areEqual(p, Object.getPrototypeOf(o));

                // Restore
                Object.defineProperty(Object.prototype, "__proto__", { get: oldDesc.get, set: oldDesc.set });
                assert.areEqual(p, o.__proto__);

                verify__proto__enabled();
            });
        }
    },
];
testRunner.run(tests);
