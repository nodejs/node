//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Tests for...in behavior when child object shadows a prototype property with a non-enumerable shadow
// See OS bug #850013

if (this.WScript && this.WScript.LoadScriptFile) { // Check for running in ch
    this.WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");
}

function forInKeysToArray(obj) {
    var s = [];
    
    for (key in obj) { 
        s.push(key);
    }
    
    return s;
}

var tests = [
    {
        name: "Simple test of prototype property shadowed by non-enumerable property",
        body: function () {
            var proto = { x: 1 };
            var child = Object.create(proto, { x: { value: 2, enumerable: false} });
            var result = forInKeysToArray(child);
            
            assert.areEqual([], result, "for...in does not enumerate a key which is enumerable in a prototype but shadowed by a non-enumerable property");
        }
    },
    {
        name: "Multiple properties on an object with some prototype properties shadowed by non-enumerable versions",
        body: function () {
            var proto = { a: 1, b: 2, c: 3, d: 4, e: 5 };
            var child = Object.create(proto, { b: { value: 20, enumerable: false} });
            Object.defineProperty(child, 'c', { enumerable: false, value: 30 });
            child['d'] = 4;
            
            var result = forInKeysToArray(child);
            
            assert.areEqual(['d','a','e'], result, "for...in does not enumerate a key which is enumerable in a prototype but shadowed by a non-enumerable property");
        }
    },
    {
        name: "Array indices which are non-enumerable (force ES5Array object)",
        body: function () {
            var o = [0,1,2];
            o[4] = 4;
            Object.defineProperty(o, 3, { enumerable: false, value: '3' })
            var result = forInKeysToArray(o);
            
            assert.areEqual(['0','1','2','4'], result, "for...in does not enumerate non-enumerable properties, even for array indices");
        }
    },
    {
        name: "Explicitly test for...in fast path",
        body: function () {
            function test(obj, expected) {
                var result = forInKeysToArray(obj);
                result = result.concat(forInKeysToArray(obj));
                result = result.concat(forInKeysToArray(obj));
                
                assert.areEqual(expected, result, "for...in does not enumerate non-enumerable properties, even from the fast-path");
            }
            
            var o = Object.create(null);
            Object.defineProperty(o, 'a', { value: 1, enumerable: false });
            Object.defineProperty(o, 'b', { value: 2, enumerable: false });
            Object.defineProperty(o, 'c', { value: 3, enumerable: false });
            test(o, []);
            
            var o = Object.create(null);
            Object.defineProperty(o, 'a', { value: 1, enumerable: true });
            Object.defineProperty(o, 'b', { value: 2, enumerable: false });
            Object.defineProperty(o, 'c', { value: 3, enumerable: false });
            test(o, ['a','a','a']);
            
            var o = Object.create(null);
            Object.defineProperty(o, 'a', { value: 1, enumerable: false });
            Object.defineProperty(o, 'b', { value: 2, enumerable: false });
            Object.defineProperty(o, 'c', { value: 3, enumerable: true });
            test(o, ['c','c','c']);
            
            var o = Object.create(null);
            Object.defineProperty(o, 'a', { value: 1, enumerable: false });
            Object.defineProperty(o, 'b', { value: 2, enumerable: true });
            Object.defineProperty(o, 'c', { value: 3, enumerable: false });
            test(o, ['b','b','b']);
            
            var o = Object.create(null);
            Object.defineProperty(o, 'a', { value: 1, enumerable: true });
            Object.defineProperty(o, 'b', { value: 2, enumerable: false });
            Object.defineProperty(o, 'c', { value: 3, enumerable: true });
            test(o, ['a','c','a','c','a','c']);
            
            // JSON is a delay-load object
            test(JSON, []);
        }
    },
    {
        name: "Shadowing non-enumerable prototype property with an enumerable version",
        body: function () {
            var proto = Object.create(null, { x: { value: 1, enumerable: false} });
            var child = Object.create(proto, { x: { value: 2, enumerable: true} });
            var result = forInKeysToArray(child);
            
            assert.areEqual(['x'], result, "Child property shadows proto property");
        }
    },
    {
        name: "Shadowing non-enumerable prototype property with another non-enumerable version",
        body: function () {
            var proto = Object.create(null, { x: { value: 1, enumerable: false} });
            var child = Object.create(proto, { x: { value: 2, enumerable: false} });
            var result = forInKeysToArray(child);
            
            assert.areEqual([], result, "Child property shadows proto property with another non-enumerable property");
        }
    },
    {
        name: "Enumerating RegExp constructor is a bit of a special case",
        body: function() {
            var result = forInKeysToArray(RegExp);
            assert.areEqual(['$1','$2','$3','$4','$5','$6','$7','$8','$9','input','rightContext','leftContext','lastParen','lastMatch'], result, "for..in of RegExp constructor returns some special properties");
            
            var result = Object.keys(RegExp);
            assert.areEqual(['$1','$2','$3','$4','$5','$6','$7','$8','$9','input','rightContext','leftContext','lastParen','lastMatch'], result, "Object.keys returns the same set of properties for RegExp as for..in");
            
            var result = Object.getOwnPropertyNames(RegExp);
            assert.areEqual(['$1','$2','$3','$4','$5','$6','$7','$8','$9','input','rightContext','leftContext','lastParen','lastMatch','length','prototype','name','$_','$&','$+','$`',"$'",'index'], result, "Object.getOwnPropertyNames returns special non-enumerable properties too");
        }
    },
    {
        name: "Multiple objects in prototype chain with enum and non-enum property shadowing",
        body: function() {
            var proto = Object.create(null, { 
                a: { value: 1, enumerable: false},
                b: { value: 1, enumerable: true},
                c: { value: 1, enumerable: false},
                d: { value: 1, enumerable: false},
                w: { value: 1, enumerable: true},
                x: { value: 1, enumerable: false},
                y: { value: 1, enumerable: true},
                z: { value: 1, enumerable: true},
            });
            var child = Object.create(proto, { 
                a: { value: 2, enumerable: false},
                b: { value: 2, enumerable: false},
                c: { value: 2, enumerable: true},
                d: { value: 2, enumerable: false},
                w: { value: 2, enumerable: true},
                x: { value: 2, enumerable: true},
                y: { value: 2, enumerable: false},
                z: { value: 2, enumerable: true},
            });
            var childchild = Object.create(child, { 
                a: { value: 3, enumerable: false},
                b: { value: 3, enumerable: false},
                c: { value: 3, enumerable: false},
                d: { value: 3, enumerable: true},
                w: { value: 3, enumerable: false},
                x: { value: 3, enumerable: true},
                y: { value: 3, enumerable: true},
                z: { value: 3, enumerable: true},
            });
            
            var result = forInKeysToArray(childchild);
            assert.areEqual(['d','x','y','z'], result, "childchild should shadow all properties and disable enumerable properties from the prototype chain leaking out");
            
            var result = forInKeysToArray(child);
            assert.areEqual(['c','w','x','z'], result, "child should shadow all properties and disable enumerable properties from the prototype chain leaking out");
            
            var result = forInKeysToArray(proto);
            assert.areEqual(['b','w','y','z'], result, "proto doesn't shadow any properties but non-enumerable properties should not show up in for..in loop");
        }
    },
    {
        name: "OS: 1905906 - Enumerating a type and alternating non-enumerable properties causes assert",
        body: function() {
            function foo() { JSON.stringify(arguments); }
            foo();
            var arr = [];
            function foo2() {
                for(var i in arguments) {
                    arr.push(i);
                }
            }
            foo2('a','b');
            
            assert.areEqual(['0','1'], arr, "Correct values are enumerated via for...in loop");
        }
    },
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
