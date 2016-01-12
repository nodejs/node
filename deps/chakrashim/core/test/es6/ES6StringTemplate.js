//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// ES6 String Template tests -- verifies the API shape and basic functionality

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

function ReturnString(str) {
    return str;
}

function GetCallsite(callsite) {
    return callsite;
}

function GetExpectedCachedCallsite() {
    return GetCallsite`some string template ${''} with replacements ${''}`;
}

function GetRawStringValue(callsite, idx) {
    return callsite.raw[idx];
}

function GetStringValue(callsite, idx) {
    return callsite[idx];
}

function tagReturnConstructor(callsite) {
    switch(callsite[0]) {
        case 'string':
            return String;
        case 'symbol':
            return Symbol;
        case 'array':
            return Array;
        case 'number':
            return Number;
    }

    return function() {
        return {
            name: 'constructed object'
        }
    };
}
function tagReturnConstructorWrapper() {
    return tagReturnConstructor;
}

var ObjectWithToString = {
    toString: function() { return "ObjectWithToString.toString() called!"; },
    value: 'ObjectWithToString.value (should not show up)'
};

var tests = [
    {
        name: "String constructor should have spec defined built-ins with correct lengths",
        body: function () {
            assert.isTrue(String.hasOwnProperty('raw'), "String constructor should have a raw method");
            assert.isTrue(String.raw.length === 1, "String.raw method takes 1 arguments");
        }
    },
    {
        name: "String.raw takes a callsite object and replacement values and returns a string that is the combination of the callsite raw strings and replacement values",
        body: function () {
            assert.throws(function () { String.raw.call(); }, TypeError, "String.raw throws TypeError if it is given no arguments", "String.raw: argument is not an Object");
            assert.throws(function () { String.raw.call('non-object'); }, TypeError, "String.raw throws TypeError if it is given non-object parameter", "String.raw: argument is not an Object");
            assert.throws(function () { String.raw.call({}); }, TypeError, "String.raw throws TypeError if it is given object which doesn't contain a raw property", "String.raw: argument is not an Object");
            assert.throws(function () { String.raw.call({ raw: undefined }); }, TypeError, "String.raw throws TypeError if it is given object which contains a raw property which is undefined", "String.raw: argument is not an Object");
            assert.throws(function () { String.raw.call({ raw: 'non-object' }); }, TypeError, "String.raw throws TypeError if it is given object which contains a raw property which is not an object", "String.raw: argument is not an Object");

            assert.areEqual("", String.raw``, "String.raw of empty string is the empty string");
            assert.areEqual("no replacement", String.raw`no replacement`, "no replacement expressions");
            assert.areEqual("\\u1234", String.raw`\u1234`, "unicode escape sequence");
            assert.areEqual("str\\ning1 simple replacement... \\r\\nstring end", String.raw`str\ning1 ${'simple replacement...'} \r\nstring end`, "string with replacement and escape sequences");
            assert.areEqual("str1\\\\ str2 str3\\\\ str4 str5\\\\ str6 str7\\\\", String.raw`str1\\ ${'str2'} str3\\ ${'str4'} str5\\ ${'str6'} str7\\`, "several replacements and escape sequences");
            assert.areEqual("begin (nested template here) end", String.raw`begin ${`(nested ${ReturnString('template')} here)`} end`, "nested string template literal");
            assert.areEqual("call toString... ObjectWithToString.toString() called! success...?", String.raw`call toString... ${ObjectWithToString} success...?`, "replacement expression is object with toString");
            assert.areEqual("simple raw string", String.raw({ raw: ["simple raw string"] }), "pass a custom object with raw property");
            assert.areEqual("string1 replacement string2", String.raw({ raw: ["string1 ", " string2"] }, ReturnString('replacement')), "pass a custom object with raw property and replacement parameter");
            assert.areEqual("string1 r1 string2", String.raw({ raw: ["string1 ", " string2"] }, 'r1', 'r2'), "pass object manually with more replacement parameters than expected");
            assert.areEqual("string1  string2", String.raw({ raw: ["string1 ", " string2"] }), "pass object manually with fewer replacement parameters than expected");
            assert.areEqual("string1  string2  string3", String.raw({ raw: ["string1 ", " string2 ", " string3"] }), "pass object manually with fewer replacement parameters than expected");
            assert.areEqual("", String.raw({ raw: [] }), "pass object manually containing with no raw strings");
        }
    },
    {
        name: "Callsite object passed to tag functions should have correct attributes",
        body: function() {
            var callsite = GetCallsite`simple template ${'with'} some ${'replacement'} expressions`;
            var descriptor;

            for (var i = 0; i < callsite.length; i++) {
                descriptor = Object.getOwnPropertyDescriptor(callsite, i);

                assert.isFalse(descriptor.writable, `callsite[${i}].descriptor.writable == false`);
                assert.isTrue(descriptor.enumerable, `callsite[${i}].descriptor.enumerable == true`);
                assert.isFalse(descriptor.configurable, `callsite[${i}].descriptor.configurable == false`);

                descriptor = Object.getOwnPropertyDescriptor(callsite.raw, i);

                assert.isFalse(descriptor.writable, `callsite.raw[${i}].descriptor.writable == false`);
                assert.isTrue(descriptor.enumerable, `callsite.raw[${i}].descriptor.enumerable == true`);
                assert.isFalse(descriptor.configurable, `callsite.raw[${i}].descriptor.configurable == false`);
            }

            descriptor = Object.getOwnPropertyDescriptor(callsite, 'raw');

            assert.isFalse(descriptor.writable, `callsite.raw.descriptor.writable == false`);
            assert.isFalse(descriptor.enumerable, `callsite.raw.descriptor.enumerable == false`);
            assert.isFalse(descriptor.configurable, `callsite.raw.descriptor.configurable == false`);

            assert.isTrue(Object.isFrozen(callsite), "callsite object is frozen");
            assert.isTrue(Object.isFrozen(callsite.raw), "callsite.raw object is frozen");
        }
    },
    {
        name: "Each string template literal corresponds to exactly one (cached) callsite object",
        body: function() {
            var callsite1 = GetCallsite`simple template literal 1`;
            var callsite2 = GetCallsite`simple template literal 2`;

            assert.isFalse(callsite1 === callsite2, "different string template literals create different callsite objects");

            var callsite3 = GetCallsite`simple template literal 3`;
            var callsite4 = GetCallsite`simple template literal 3`;

            assert.isTrue(callsite3 === callsite4, "different string template literals with the same string literal value create identical callsite objects");

            var loopCallsite = undefined;
            for (var i = 0; i < 10; i++) {
                var c = GetCallsite`loop template literal ${i}`;

                if (loopCallsite === undefined) {
                    loopCallsite = c;
                } else {
                    assert.isTrue(loopCallsite === c, "string template literal used in a loop reuses the same callsite object.");
                }

                assert.areEqual(2, c.length, "loop callsite has expected count of string literals");
                assert.areEqual("loop template literal ", c[0], "loop callsite has expected first string literal value");
                assert.areEqual("", c[1], "loop callsite has expected second string literal value");

                assert.areEqual(2, c.raw.length, "loop callsite.raw has expected count of string literals");
                assert.areEqual("loop template literal ", c.raw[0], "loop callsite.raw has expected first string literal value");
                assert.areEqual("", c.raw[1], "loop callsite.raw has expected second string literal value");
            }

            loopCallsite = undefined
            for (var i = 0; i < 10; i++) {
                var c = GetExpectedCachedCallsite();

                if (loopCallsite === undefined) {
                    loopCallsite = c;
                } else {
                    assert.isTrue(loopCallsite === c, "string template declared in other function returns same callsite object when function called.");
                }

                assert.areEqual(3, c.length, "loop callsite has expected count of string literals");
                assert.areEqual("some string template ", c[0], "loop callsite has expected first string literal value");
                assert.areEqual(" with replacements ", c[1], "loop callsite has expected second string literal value");
                assert.areEqual("", c[2], "loop callsite has expected third string literal value");

                assert.areEqual(3, c.raw.length, "loop callsite.raw has expected count of string literals");
                assert.areEqual("some string template ", c.raw[0], "loop callsite.raw has expected first string literal value");
                assert.areEqual(" with replacements ", c.raw[1], "loop callsite.raw has expected second string literal value");
                assert.areEqual("", c.raw[2], "loop callsite.raw has expected third string literal value");
            }
        }
    },
    {
        name: "BLUE: 490848 - string templates do not allow substitution expressions which contain '}' characters",
        body: function() {
            var foo;

            assert.areEqual("function () { return 'foo called'; }", `${foo = function() { return 'foo called'; }}`, "Function declaration (+assignment) in string template");
            assert.areEqual('function', typeof foo, "Assignment inside string template substitution expression");
            assert.areEqual('foo called', foo(), "Function declared in template expression can be called later");

            assert.areEqual("ObjectLiteral.toString() called", `${{toString:function(){return 'ObjectLiteral.toString() called';}}}`, "Object literal declared in string template expression");

            foo = undefined;

            assert.areEqual("foo: comma syntax", `${(foo = function() { return 'foo: comma syntax'; }, foo())}`, "Comma syntax in string template");
            assert.areEqual('function', typeof foo, "Assignment inside string template substitution expression");
            assert.areEqual('foo: comma syntax', foo(), "Function declared in template expression can be called later");

            assert.areEqual('{toString}', `{${{toString:function(){return 'toString';}}}}`, "Test all the permutations of '}' characters in a string template");
        }
    },
    {
        name: "BLUE: 525727 - using a let declaration inside a string template causes an assertion",
        body: function() {
            assert.throws(function () { eval('`${let x = 5}`;'); }, SyntaxError, "Let returns an identifier - can't be a substitution expression", "Expected '}'");
            assert.throws(function () { eval('`${a a}`;'); }, SyntaxError, "ParseExpr returns the first word as an identifier.", "Expected '}'");
        }
    },
    {
        name: "BLUE: 557210 - string templates do not check for escaped uses of substitution expressions",
        body: function() {
            // None of these should cause AV
            new Function("function z() {}; `z`;")();
            new Function("function z() {}; `${z}`;")();
            new Function("function z() {}; `${z}${z}`;")();
            new Function("function z() {}; `${z}${z}${z}`;")();
            new Function("function z() {}; `${'z'}${z}${z}`;")();
            new Function("function z() {}; `${'z'}${'z'}${z}`;")();
            new Function("function z() {}; '' + z + '';")();
            new Function("function z() {}; z`${`${z}`}`;")();
            new Function("function z() {}; z``;")();
            new Function("function z() {}; ``;")();

            new Function("(`${function(id) { return id }}`);")();
            new Function("function y() {} y`${`${'z'}${`${function(id) { return id }})${ /x/g >= 'c'}`}`}`;")();
        }
    },
    {
        name: "BLUE: 560073 - parsing a tagged string template does not respect the fInNew flag",
        body: function() {
            assert.throws(function() { new tagReturnConstructor`symbol`; }, TypeError, "Calling 'new Symbol()' throws so we should see a throw if we try to new the return from the tag function if it is Symbol", "Function is not a constructor");

            assert.isTrue(Array.isArray(new tagReturnConstructor`array`), "Simple case of calling tagged string template as constructor");
            assert.areEqual(6, new tagReturnConstructor`array${`string1`}${`string2`}`(6).length, "Calling tagged string template as constructor with replacements and arguments");

            assert.areEqual(`string`, new tagReturnConstructor`string`(`STRING`).toLowerCase(), "Create a string and pass parameters via String constructor returned from tag function");

            assert.throws(function() { new tagReturnConstructorWrapper`unused``symbol`; }, TypeError, "Calling 'new Symbol()' throws so we should see a throw if we try to new the return from the tag function if it is Symbol", "Function is not a constructor");

            assert.areEqual(new Number(0), new tagReturnConstructorWrapper`unused``number`, "Create a boxed number object via Number constructor returned from previous string template tag function");
            assert.areEqual(Object(`actual string value`), new tagReturnConstructorWrapper`unused``string`(`actual string value`), "Create a string via String constructor returned from previous string template tag function");
        }
    },
    {
        name: "String template tag function returning a constructor function should be constructable in a new expression",
        body: function() {
            function tagReturnNestedConstructorFunction(name) {
                return tagReturnConstructor([name]);
            }
            function tagReturnNestedConstructorObject(callsite) {
                return {
                    constructor_array : [ Symbol, String, Array, Number ],
                    constructor_tag_function : tagReturnConstructor,
                    constructor_ordinary_function : tagReturnNestedConstructorFunction,
                    constructor_object : { symbol : Symbol, string : String, array : Array, number : Number },
                    constructor_nested : function() { return tagReturnNestedConstructorObject; }
                };
            }
            function tagReturnNestedConstructorArray() {
                return tagReturnNestedConstructorObject().constructor_array;
            }
            function tagReturnNestedConstructorArrayWrapper() {
                return [function() { return tagReturnNestedConstructorArray(); }];
            }

            assert.throws(function() { new tagReturnNestedConstructorObject`ignored`.constructor_array[0]; }, TypeError, "Calling 'new Symbol()' throws so we should see a throw if we try to new the return from the tag function if it is Symbol", "Function is not a constructor");
            assert.throws(function() { new tagReturnNestedConstructorObject`ignored`.constructor_tag_function`symbol`; }, TypeError, "Calling 'new Symbol()' throws so we should see a throw if we try to new the return from the tag function if it is Symbol", "Function is not a constructor");

            assert.isTrue(Symbol === new tagReturnNestedConstructorObject`ignored`.constructor_ordinary_function(`symbol`), "Mixing MemberExpression token types");
            assert.throws(function() { new new tagReturnNestedConstructorObject`ignored`.constructor_ordinary_function(`symbol`); }, TypeError, "new constructor_ordinary_function returns Symbol which should throw if new'd", "Function is not a constructor");

            assert.isTrue(Symbol === new tagReturnNestedConstructorObject`ignored`.constructor_nested(`also ignored`)`one more ignored thing`.constructor_object.symbol, `Mixing MemberExpression token types`);
            assert.isTrue(Array.isArray(new new tagReturnNestedConstructorObject`ignored`.constructor_nested(`also ignored`)`one more ignored thing`.constructor_object.array), `Mixing MemberExpression token types`);
            assert.areEqual(`object`, typeof new new tagReturnNestedConstructorObject`ignored`.constructor_nested(`also ignored`)`one more ignored thing`.constructor_object.number(42), `Mixing MemberExpression token types`);
            assert.areEqual(`real string`, new new tagReturnNestedConstructorObject`ignored`.constructor_nested(`also ignored`)`one more ignored thing`.constructor_object.string(`Real String`).toLowerCase(), `Mixing MemberExpression token types`);

            assert.isTrue(Symbol === new tagReturnNestedConstructorObject`ignored`[`constructor_nested`](`also ignored`)`one more ignored thing`.constructor_object.symbol, `Mixing MemberExpression token types`);
            assert.isTrue(Array.isArray(new new tagReturnNestedConstructorObject`ignored`[`constructor_nested`](`also ignored`)`one more ignored thing`.constructor_object.array), `Mixing MemberExpression token types`);
            assert.areEqual(`object`, typeof new new tagReturnNestedConstructorObject`ignored`[`constructor_nested`](`also ignored`)`one more ignored thing`.constructor_object.number(42), `Mixing MemberExpression token types`);
            assert.areEqual(`real string`, new new tagReturnNestedConstructorObject`ignored`[`constructor_nested`](`also ignored`)`one more ignored thing`.constructor_object.string(`Real String`).toLowerCase(), `Mixing MemberExpression token types`);

            assert.isTrue(Symbol === tagReturnNestedConstructorObject`ignored`[`constructor_nested`]`also ignored``one more ignored thing`[`constructor_object`].symbol, `Mixing MemberExpression token types`);
            assert.isTrue(Array.isArray(new tagReturnNestedConstructorObject`ignored`[`constructor_nested`]`also ignored``one more ignored thing`[`constructor_object`].array), `Mixing MemberExpression token types`);
            assert.areEqual(`object`, typeof new tagReturnNestedConstructorObject`ignored`[`constructor_nested`]`also ignored``one more ignored thing`[`constructor_object`].number(42), `Mixing MemberExpression token types`);
            assert.areEqual(`real string`, new tagReturnNestedConstructorObject`ignored`[`constructor_nested`]`also ignored``one more ignored thing`[`constructor_object`].string(`Real String`).toLowerCase(), `Mixing MemberExpression token types`);

            assert.isTrue(Symbol === tagReturnNestedConstructorObject`ignored`[`constructor_nested`]`also ignored``one more ignored thing`[`constructor_object`][`symbol`], `Mixing MemberExpression token types`);
            assert.isTrue(Array.isArray(new tagReturnNestedConstructorObject`ignored`[`constructor_nested`]`also ignored``one more ignored thing`[`constructor_object`][`array`]), `Mixing MemberExpression token types`);
            assert.areEqual(`object`, typeof new tagReturnNestedConstructorObject`ignored`[`constructor_nested`]`also ignored``one more ignored thing`[`constructor_object`][`number`](42), `Mixing MemberExpression token types`);
            assert.areEqual(`real string`, new tagReturnNestedConstructorObject`ignored`[`constructor_nested`]`also ignored``one more ignored thing`[`constructor_object`][`string`](`Real String`).toLowerCase(), `Mixing MemberExpression token types`);

            assert.throws(function() { new tagReturnNestedConstructorArray`ignored`[0]; }, TypeError, `Array access returns Symbol which throws when called as new expression`, `Function is not a constructor`);
            assert.isTrue(Array.isArray(new tagReturnNestedConstructorArray`ignored`[2]), `Array access returning Array used in a new expression`);
            assert.areEqual(Object(42), new tagReturnNestedConstructorArray`ignored`[3](42), `Array access returning Number used in a new expression`);

            assert.isTrue(Array.isArray(new tagReturnNestedConstructorArrayWrapper`ignored`[0]`also ignored`[2]), `Array access returning Array used in a new expression`);
            assert.areEqual(`string`, new tagReturnNestedConstructorArrayWrapper`ignored`[0]`also ignored`[1](`STRING`).toLowerCase(), `Array access returning String used in a new expression`);

            assert.areEqual(`string objectstring literal,,,`, String.raw`${new tagReturnNestedConstructorArray`ignored${String.raw`nothing`}`[1](`string object`)}string literal${new tagReturnNestedConstructorObject`ignored`[`constructor_object`][`array`](4)}`, `Nested string templates can have their own new expressions`);

            function tagReturnNestedConstructorObjectWrapper(callsite, expr1, expr2) {
                assert.areEqual([`string literal (pre)`,`string literal (middle)`,`string literal (post)`], callsite, `Callsite cooked string constants are correct`);
                assert.areEqual([`string literal (pre)`,`string literal (middle)`,`string literal (post)`], callsite.raw, `Callsite raw string constants are correct`);
                assert.areEqual(Object(`s1bprereplacement1s1bpost`), expr1, `First replacement expression is created via 'new String("...")'`);
                assert.isTrue(Array.isArray(expr2), `Second replacement expression is created via Array constructor`)
                assert.areEqual(new Array(6), expr2, `Second replacement expression has the right size and contents`);

                return function() { return tagReturnNestedConstructorObject(); }
            }

            assert.isTrue(Array.isArray(
                new tagReturnNestedConstructorObjectWrapper
                    `string literal (pre)${new tagReturnNestedConstructorArray`ignored${String.raw`ignored replacement`}ignored`[1](`s1bpre${String.raw`replacement1`}s1bpost`)}string literal (middle)${new tagReturnNestedConstructorObject`s2pre${new Number(42)}s2post`[`constructor_object`][`array`](6)}string literal (post)``ignored`
                    [`constructor_object`][`array`]
                ),
                `Many-levels of nested string templates can all have different expression types - NewExpressions, MemberExpressions, PrimaryExpressions`
            )

            var _counter = 0;
            function nestedNewOperatorFunction(callsite) {
                if (_counter > 2) {
                    return tagReturnNestedConstructorFunction(callsite ? callsite[0] : ``);
                } else {
                    _counter++;
                    return nestedNewOperatorFunction;
                }
            }

            var obj = new new new new new nestedNewOperatorFunction
            assert.areEqual(`constructed object`, obj.name, `Calling nested constructor helper with no arguments eventually returns a constructor which returns an object with name property`);

            _counter = 0;
            assert.areEqual(new String(`help...`), new new new new new nestedNewOperatorFunction(`1`)(`2`)(`3`)([`string`])(`help...`), `Nested constructor wrapper chooses String builtin constructor`)

            _counter = 0;
            assert.isTrue(Array.isArray(new nestedNewOperatorFunction`1``2``3``array`), `Nested constructor wrapper is called inherently by string template evaluation - empty arguments`)

            _counter = 0;
            assert.areEqual(new String(`help...`), new nestedNewOperatorFunction`1``2``3``string`(`help...`), `Nested constructor wrapper is called inherently by string template evaluation - passing arguments to NewExpression`)
        }
    },
    {
        name: "Multiple post-fix operators in a row including string templates",
        body: function() {
            function foo() {
                return { 'something' : function() { return ['v1','v2','v3']; } };
            }
            assert.areEqual('v3', foo`'nothing'`['something']()[2], "Multiple post-fix operators after a string template");

            function tag(callsite, replacement) {
                return {
                    v : callsite[0] + " " + replacement,
                    f : function() { return ['a1', function(s) { return s; }]; }
                };
            }

            assert.areEqual('another string template - ', tag`a ${'b'} c`.f()[1]`another string template - ${'with replacements'}`[0], "Multiple post-fix operators including multiple string templates");
        }
    },
    {
        name: "String template callsite objects are unique per-Realm and indexed by the raw strings",
        body: function() {
            var callsite = undefined;
            var counter = 0;

            function tag(c)
            {
                counter++;

                assert.areEqual('uniquestringforrealmcachetest\\n', c.raw[0], 'String template callsite has correct raw value');

                if (callsite === undefined) {
                    callsite = c;
                } else {
                    assert.isTrue(c === callsite, 'Callsite is correctly cached per-Realm');
                }
            }

            function foo() {
                tag`uniquestringforrealmcachetest\n`;
                tag`uniquestringforrealmcachetest\n`;
            }

            foo();
            foo();

            function foo2() {
                tag`uniquestringforrealmcachetest\n`;
                tag`uniquestringforrealmcachetest\n`;
            }

            foo2();
            foo2();

            function foo3() {
                eval('tag`uniquestringforrealmcachetest\\n`');
                eval('tag`uniquestringforrealmcachetest\\n`');
            }

            foo3();
            foo3();

            counter = 0;

            var foo4 = new Function('t','t`uniquestringforrealmcachetest\\n`;');

            foo4(tag);
            foo4(tag);

            assert.areEqual(2, counter, "tag function is called correct number of times");

            counter = 0;

            var foo5 = new Function('t','eval("t`uniquestringforrealmcachetest\\\\n`;");');

            foo5(tag);
            foo5(tag);

            assert.areEqual(2, counter, "tag function is called correct number of times");
        }
    },
    {
        name: "Callsite objects are not strict equal if raw strings differ (even when cooked strings are strict equal)",
        body: function() {
            var callsite1 = GetCallsite`before
after`;
            var callsite2 = GetCallsite`before\nafter`;

            assert.areEqual("before\nafter", callsite1[0], 'Explicit line terminator character is copied directly into cooked strings');
            assert.areEqual("before\nafter", callsite2[0], 'Escaped line terminator is translated into cooked strings');
            assert.areEqual("before\nafter", callsite1.raw[0], 'Explicit line terminator character is copied directly into raw strings');
            assert.areEqual("before\\nafter", callsite2.raw[0], 'Escaped line terminator is re-escaped into raw strings');
            assert.isTrue(callsite1[0] === callsite2[0], 'Cooked strings are strictly equal');
            assert.isFalse(callsite1.raw[0] === callsite2.raw[0], 'Raw strings are not strictly equal');
            assert.isFalse(callsite1 === callsite2, 'Callsite objects are not the same ');
        }
    },
    {
        name: "Callsite objects are constant even if replacement values differ",
        body: function() {
            var callsite1 = GetCallsite`string1${'r1'}string2${'r2'}string3`;
            var callsite2 = GetCallsite`string1${'r3'}string2${'r4'}string3`;

            assert.isTrue(callsite1 === callsite2, "Callsite objects are strictly equal");
        }
    },
    {
        name: "Octal escape sequences are not allowed in string template literals",
        body: function() {
            assert.throws(function () { eval('print(`\\00`)'); }, SyntaxError, "Scanning an octal escape sequence throws SyntaxError.", "Octal numeric literals and escape characters not allowed in strict mode");
            assert.throws(function () { eval('print(`\\01`)'); }, SyntaxError, "Scanning an octal escape sequence throws SyntaxError.", "Octal numeric literals and escape characters not allowed in strict mode");
            assert.throws(function () { eval('print(`\\1`)'); }, SyntaxError, "Scanning an octal escape sequence throws SyntaxError.", "Octal numeric literals and escape characters not allowed in strict mode");
            assert.throws(function () { eval('print(`\\2`)'); }, SyntaxError, "Scanning an octal escape sequence throws SyntaxError.", "Octal numeric literals and escape characters not allowed in strict mode");
            assert.throws(function () { eval('print(`\\3`)'); }, SyntaxError, "Scanning an octal escape sequence throws SyntaxError.", "Octal numeric literals and escape characters not allowed in strict mode");
            assert.throws(function () { eval('print(`\\4`)'); }, SyntaxError, "Scanning an octal escape sequence throws SyntaxError.", "Octal numeric literals and escape characters not allowed in strict mode");
            assert.throws(function () { eval('print(`\\5`)'); }, SyntaxError, "Scanning an octal escape sequence throws SyntaxError.", "Octal numeric literals and escape characters not allowed in strict mode");
            assert.throws(function () { eval('print(`\\6`)'); }, SyntaxError, "Scanning an octal escape sequence throws SyntaxError.", "Octal numeric literals and escape characters not allowed in strict mode");
            assert.throws(function () { eval('print(`\\7`)'); }, SyntaxError, "Scanning an octal escape sequence throws SyntaxError.", "Octal numeric literals and escape characters not allowed in strict mode");
            assert.throws(function () { eval('print(`\\10`)'); }, SyntaxError, "Scanning an octal escape sequence throws SyntaxError.", "Octal numeric literals and escape characters not allowed in strict mode");
            assert.throws(function () { eval('print(`\\50`)'); }, SyntaxError, "Scanning an octal escape sequence throws SyntaxError.", "Octal numeric literals and escape characters not allowed in strict mode");
            assert.throws(function () { eval('print(`\\30`)'); }, SyntaxError, "Scanning an octal escape sequence throws SyntaxError.", "Octal numeric literals and escape characters not allowed in strict mode");
            assert.throws(function () { eval('print(`\\70`)'); }, SyntaxError, "Scanning an octal escape sequence throws SyntaxError.", "Octal numeric literals and escape characters not allowed in strict mode");
            assert.throws(function () { eval('print(`\\123`)'); }, SyntaxError, "Scanning an octal escape sequence throws SyntaxError.", "Octal numeric literals and escape characters not allowed in strict mode");
            assert.throws(function () { eval('print(`\\377`)'); }, SyntaxError, "Scanning an octal escape sequence throws SyntaxError.", "Octal numeric literals and escape characters not allowed in strict mode");

            assert.throws(function () { eval('print(`\\0123`)'); }, SyntaxError, "Scanning an octal escape sequence throws SyntaxError.", "Octal numeric literals and escape characters not allowed in strict mode");
            assert.throws(function () { eval('print(`\\567`)'); }, SyntaxError, "Scanning an octal escape sequence throws SyntaxError.", "Octal numeric literals and escape characters not allowed in strict mode");
        }
    },
    {
        name: "Extended unicode escape sequences",
        body: function() {
            assert.areEqual("a\u{d}c", `a\u{d}c`, "Cooked value for extended unicode escape sequence of 1 hex character");
            assert.areEqual("a\\u{d}c", GetRawStringValue`a\u{d}c${0}`, "Raw value for extended unicode escape sequence of 1 hex character");

            assert.areEqual("a\u{62}c", `a\u{62}c`, "Cooked value for extended unicode escape sequence of 2 hex characters");
            assert.areEqual("a\\u{62}c", GetRawStringValue`a\u{62}c${0}`, "Raw value for extended unicode escape sequence of 2 hex characters");

            assert.areEqual("a\u{062}c", `a\u{062}c`, "Cooked value for extended unicode escape sequence of 3 hex characters");
            assert.areEqual("a\\u{062}c", GetRawStringValue`a\u{062}c${0}`, "Raw value for extended unicode escape sequence of 3 hex characters");

            assert.areEqual("a\u{0062}c", `a\u{0062}c`, "Cooked value for extended unicode escape sequence of 4 hex characters");
            assert.areEqual("a\\u{0062}c", GetRawStringValue`a\u{0062}c${0}`, "Raw value for extended unicode escape sequence of 4 hex characters");

            assert.areEqual("a\u{00062}c", `a\u{00062}c`, "Cooked value for extended unicode escape sequence of 5 hex characters");
            assert.areEqual("a\\u{00062}c", GetRawStringValue`a\u{00062}c${0}`, "Raw value for extended unicode escape sequence of 5 hex characters");

            assert.areEqual("a\u{000062}c", `a\u{000062}c`, "Cooked value for extended unicode escape sequence of 6 hex characters");
            assert.areEqual("a\\u{000062}c", GetRawStringValue`a\u{000062}c${0}`, "Raw value for extended unicode escape sequence of 6 hex characters");

            assert.areEqual("a\u{00000062}c", `a\u{00000062}c`, "Cooked value for extended unicode escape sequence of 7 hex characters");
            assert.areEqual("a\\u{00000062}c", GetRawStringValue`a\u{00000062}c${0}`, "Raw value for extended unicode escape sequence of 7 hex characters");

            assert.areEqual("a\u{000000062}c", `a\u{000000062}c`, "Cooked value for extended unicode escape sequence of 8 hex characters");
            assert.areEqual("a\\u{000000062}c", GetRawStringValue`a\u{000000062}c${0}`, "Raw value for extended unicode escape sequence of 8 hex characters");
        }
    },
    {
        name: "Template value for escaped null characters",
        body: function() {
            assert.areEqual("\0", eval("GetRawStringValue`\0${0}`"), "The template raw value of <NULL> is <NULL>");
            assert.areEqual("\\\u0030", eval("GetRawStringValue`\\\0${0}`"), "The template raw value of \\<NULL> is \\ + \u0030");

            assert.areEqual("\0", eval("GetStringValue`\0${0}`"), "The template cooked value of <NULL> is <NULL>");
            assert.areEqual("\0", eval("GetStringValue`\\\0${0}`"), "The template cooked value of \\<NULL> is <NULL>");
        }
    },
    {
        name: "String template line terminator sequence normalization",
        body: function() {
            assert.isTrue(eval("GetStringValue`\n${0}`") === "\n", "String template literal doesn't normalize <LF> line terminator sequence"); // `<LF>` => '<LF>'
            assert.isTrue(eval("GetStringValue`\\\n${0}`") === "", "String template literal ignores escaped <LF> line continuation token"); // `\<LF>` => ''
            assert.isTrue(eval("GetStringValue`\\n${0}`") === "\n", "String template literal doesn't normalize <LF> escape sequence"); // `\n` => '<LF>'

            assert.isTrue(eval("GetStringValue`\r${0}`") === "\n", "String template literal normalizes <CR> line terminator sequence to <LF>"); // `<CR>` => '<LF>'
            assert.isTrue(eval("GetStringValue`\\\r${0}`") === "", "String template literal ignores escaped <CR> line continuation token"); // `\<CR>` => ''
            assert.isTrue(eval("GetStringValue`\\r${0}`") === "\r", "String template literal doesn't normalize <CR> escape sequence"); // `\r` => '<CR>'

            assert.isTrue(eval("GetStringValue`\r\n${0}`") === "\n", "String template literal normalizes <CR><LF> line terminator sequence to <LF>"); // `<CR><LF>` => '<LF>'
            assert.isTrue(eval("GetStringValue`\\\r\\\n${0}`") === "", "String template literal ignores separately escaped <CR><LF> line continuation sequence tokens"); // `\<CR>\<LF>` => ''
            assert.isTrue(eval("GetStringValue`\\\r\n${0}`") === "", "String template literal ignores escaped <CR><LF> line continuation sequence"); // `\<CR><LF>` => ''
            assert.isTrue(eval("GetStringValue`\r\\\n${0}`") === "\n", "String template literal normalizes <CR> line terminator sequence if the next character is <LF> line continuation sequence"); // `<CR>\<LF>` => '<LF>'
            assert.isTrue(eval("GetStringValue`\\r\\n${0}`") === "\r\n", "String template literal doesn't normalize <CR><LF> as escape sequences"); // `\r\n` => '<CR><LF>'
            assert.isTrue(eval("GetStringValue`\\r\n${0}`") === "\r\n", "String template literal doesn't normalize <CR> as an escape sequence if the next character is <LF> line terminator sequence"); // `\r<LF>` => '<CR><LF>'
            assert.isTrue(eval("GetStringValue`\r\\n${0}`") === "\n\n", "String template literal normalizes <CR> as a line terminator sequence if the next character is <LF> escape sequence"); // `<CR>\n` => '<LF><LF>'
            assert.isTrue(eval("GetStringValue`\\\r\\n${0}`") === "\n", "String template literal ignores <CR> as a line continuation if the next character is <LF> escape sequence"); // `\<CR>\n` => '<LF>'
            assert.isTrue(eval("GetStringValue`\\r\\\n${0}`") === "\r", "String template literal doesn't normalize <CR> as an escape sequence if the next character is <LF> line continuation token"); // `\r\<LF>` => '<CR>'

            assert.isTrue(eval("GetStringValue`\u2028${0}`") === "\u2028", "String template literal doesn't normalize <LS> line terminator sequence"); // `<LS>` => '<LS>'
            assert.isTrue(eval("GetStringValue`\\\u2028${0}`") === "", "String template literal ignores escaped <LS> line continuation token"); // `\<LS>` => ''

            assert.isTrue(eval("GetStringValue`\u2029${0}`") === "\u2029", "String template literal doesn't normalize <PS> line terminator sequence"); // `<PS>` => '<PS>'
            assert.isTrue(eval("GetStringValue`\\\u2029${0}`") === "", "String template literal ignores escaped <PS> line continuation token"); // `\<PS>` => ''
        }
    },
    {
        name: "String template line terminator sequence normalization in raw string values",
        body: function() {
            assert.isTrue(eval("GetRawStringValue`\n${0}`") === '\n', "String template raw literal doesn't normalize <LF> line terminator sequence"); // `<LF>`.raw => '<LF>'
            assert.isTrue(eval("GetRawStringValue`\\\n${0}`") === "\\\n", "String template raw literal doesn't normalize escaped <LF> line continuation token"); // `\<LF>`.raw => '\<LF>'
            assert.isTrue(eval("GetRawStringValue`\\n${0}`") === "\\n", "String template raw literal doesn't evaluate <LF> escape sequence"); // `\n`.raw => '\n'

            assert.isTrue(eval("GetRawStringValue`\r${0}`") === "\n", "String template raw literal normalizes <CR> line terminator sequence to <LF>"); // `<CR>`.raw => '<LF>'
            assert.isTrue(eval("GetRawStringValue`\\\r${0}`") === "\\\n", "String template raw literal normalizes escaped <CR> line continuation token to <LF>"); // `\<CR>`.raw => '\<LF>'
            assert.isTrue(eval("GetRawStringValue`\\r${0}`") === "\\r", "String template raw literal doesn't evaluate <CR> escape sequence"); // `\r`.raw => '\r'

            assert.isTrue(eval("GetRawStringValue`\r\n${0}`") === "\n", "String template raw literal normalizes <CR><LF> line terminator sequence to <LF>"); // `<CR><LF>`.raw => '<LF>'
            assert.isTrue(eval("GetRawStringValue`\\\r\\\n${0}`") === "\\\n\\\n", "String template raw literal normalizes separately escaped <CR><LF> line continuation sequence tokens"); // `\<CR>\<LF>`.raw => '\<LF>\<LF>'
            assert.isTrue(eval("GetRawStringValue`\\\r\n${0}`") === "\\\n", "String template raw literal normalizes escaped <CR><LF> line continuation sequence to <LF>"); // `\<CR><LF>`.raw => '\<LF>'
            assert.isTrue(eval("GetRawStringValue`\r\\\n${0}`") === "\n\\\n", "String template raw literal normalizes <CR> line terminator sequence if the next character is <LF> line continuation sequence"); // `<CR>\<LF>`.raw => '<LF>\<LF>'
            assert.isTrue(eval("GetRawStringValue`\\r\\n${0}`") === "\\r\\n", "String template raw literal doesn't evaluate <CR><LF> as escape sequences"); // `\r\n`.raw => '\r\n'
            assert.isTrue(eval("GetRawStringValue`\\r\n${0}`") === "\\r\n", "String template raw literal doesn't evaluate <CR> as an escape sequence if the next character is <LF> line terminator sequence"); // `\r<LF>`.raw => '\r<LF>'
            assert.isTrue(eval("GetRawStringValue`\r\\n${0}`") === "\n\\n", "String template raw literal normalizes <CR> as a line terminator sequence if the next character is <LF> escape sequence"); // `<CR>\n`.raw => '<LF>\n'
            assert.isTrue(eval("GetRawStringValue`\\\r\\n${0}`") === "\\\n\\n", "String template raw literal normalizes <CR> as a line continuation if the next character is <LF> escape sequence"); // `\<CR>\n`.raw => '\<LF>\n'
            assert.isTrue(eval("GetRawStringValue`\\r\\\n${0}`") === "\\r\\\n", "String template raw literal doesn't evaluate <CR> as an escape sequence if the next character is <LF> line continuation token"); // `\r\<LF>`.raw => '\r\<LF>'

            assert.isTrue(eval("GetRawStringValue`\u2028${0}`") === "\u2028", "String template raw literal doesn't normalize <LS> line terminator sequence"); // `<LS>`.raw => '<LS>'
            assert.isTrue(eval("GetRawStringValue`\\\u2028${0}`") === "\\\u2028", "String template raw literal doesn't normalize escaped <LS> line continuation token"); // `\<LS>`.raw => '\<LS>'

            assert.isTrue(eval("GetRawStringValue`\u2029${0}`") === "\u2029", "String template raw literal doesn't normalize <PS> line terminator sequence"); // `<PS>`.raw => '<PS>'
            assert.isTrue(eval("GetRawStringValue`\\\u2029${0}`") === "\\\u2029", "String template raw literal doesn't normalize escaped <PS> line continuation token"); // `\<PS>`.raw => '\<PS>'
        }
    },
    {
        name: "Bug fix : 4532336 String Template should trigger ToString on the substitution expression",
        body: function() {
            var a = {
                toString: function (){ return "foo";},
                valueOf: function() { return "bar";}
            };

            assert.areEqual(`${a}`, "foo", "toString should be called instead of valueOf on the substitution expression");
        }
    },
    {
        name: "String template converts `\\0` into a null character - not an octal escape sequence",
        body: function() {
            assert.areEqual('\0', `\0`, "Simple null escape sequence in string template is treated the same as in a normal string");
            assert.areEqual('\08', `\08`, "Null escape sequence followed by non-octal number is valid");
            assert.areEqual('\0abc', `\0abc`, "Null escape sequence followed by non-number is valid");
            assert.areEqual('\0\0', `\0\0`, "Null escape sequence followed by another null escape sequence is valid");

            var called = false;
            function tag(obj) {
                assert.areEqual('\0', obj[0], "Null escape sequence is cooked into null character");
                assert.areEqual('\\0', obj.raw[0], "Null escape sequence is not cooked in raw string");
                called = true;
            }

            tag`\0`;
            assert.isTrue(called);
        }
    },
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
