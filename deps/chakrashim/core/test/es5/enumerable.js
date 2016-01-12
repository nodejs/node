//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var echo = WScript.Echo;

function guarded_call(func) {
    try {
        func();
    } catch (e) {
        echo(e.name + " : " + e.message);
    }
}

var testCount = 0;
function scenario(title, func) {
    if (testCount > 0) {
        echo("\n");
    }
    echo((testCount++) + ".", title);
    guarded_call(func);
}

// If needed, echo lines sorted to remove dependency on order.
function echoLines(lines, sort) {
    if (sort) {
        lines.sort();
    }
    for(var i = 0; i < lines.length; i++) {
        echo(lines[i]);
    }
}

// dump own property descriptors of obj
function dumpProp(obj) {
    var lines = [];
    var names = Object.getOwnPropertyNames(obj);
    for (var p in names) {
        var name = names[p];
        var desc = Object.getOwnPropertyDescriptor(obj, name);
        var s = "  " + name + ": " + obj[name];
        if (desc.enumerable) { s += " enumerable"; }
        if (desc.configurable) { s += " configurable"; }
        if (desc.writable) { s += " writable"; }
        if (desc.getter) { s += " getter"; }
        if (desc.setter) { s += " setter"; }
        lines.push(s);
    }
    echoLines(lines);
}

// dump obj through for-in enumerator
function enumObj(obj) {
    var lines = [];
    for (var p in obj) {

        // add property during enumeration
        if (!obj[500]) {
            obj[500] = "Should not enumerate 500";
            obj[600] = "Should not enumerate 600";
            Object.defineProperty(obj, "700", {
                get: function () { return "Should not enumerate 700"; },
                enumerable: true,
                configurable: true
            });
        }

        lines.push("  " + p + ": " + obj[p]);
    }
    echoLines(lines);
}

// add a bunch of data/attribute properties with different attributes
function addProp(o, prefix) {
    Object.defineProperty(o, prefix + "10", {
        value: "value 10"
    });
    Object.defineProperty(o, prefix + "11", {
        value: "value 11",
        enumerable: true
    });
    Object.defineProperty(o, prefix + "12", {
        value: "value 12",
        enumerable: true,
        configurable: true
    });
    Object.defineProperty(o, prefix + "13", {
        value: "value 13",
        enumerable: true,
        configurable: true,
        writable: true
    });

    Object.defineProperty(o, prefix + "20", {
        get: function() { return "get 20"; },
    });
    Object.defineProperty(o, prefix + "21", {
        get: function () { return "get 21"; },
        enumerable: true,
    });
    Object.defineProperty(o, prefix + "22", {
        get: function () { return "get 22"; },
        enumerable: true,
        configurable: true
    });

    Object.defineProperty(o, prefix + "25", {
        set: function() { echo("do not call 25"); },
    });
    Object.defineProperty(o, prefix + "26", {
        set: function() { echo("do not call 26"); },
        enumerable: true,
    });
    Object.defineProperty(o, prefix + "27", {
        set: function() { echo("do not call 27"); },
        enumerable: true,
        configurable: true
    });
}

function testWithObj(o) {
    addProp(o, "xx");
    addProp(o, "1");
    echo("  --- properties ---");
    dumpProp(o);
    echo("  --- for-in enumerate ---");
    enumObj(o);
}

scenario("Test with object", function() {
    testWithObj({
        abc: -12,
        def: "hello",
        1: undefined,
        3: null
    });
});

scenario("Test with array", function() {
    testWithObj([
        -12, "hello", undefined, null
    ]);
});

// Test Object.defineProperties, Object.create
function testPrototype(proto) {
    Object.defineProperties(proto, {
        name: { value: "SHOULD_NOT_enumerate_prototype" },
        0: { get: function() { return "get 0"; } },
        3: { value: 3 },
        1: { get: function() { return "get 1"; }, enumerable: true },
        5: { value: 5, enumerable: true },
        2: { get: function() { return this.name; }, enumerable: true },
    });

    var o = Object.create(proto, {
        name: { value: "correct_original_instance" },
        10: { get: function() { return "get 10"; } },
        13: { value: 13 },
        11: { get: function() { return "get 11"; }, enumerable: true },
        15: { value: 15, enumerable: true },
        12: { get: function() { return this.name; }, enumerable: true },        
    });

    echo("*** Prototype ***");
    dumpProp(proto);
    echo("*** Object ***");
    dumpProp(o);
    echo("*** for in ***");
    enumObj(o);
}

scenario("Test prototype with object", function() {
    testPrototype({});
});

scenario("Test prototype with array", function() {
    testPrototype([]);
});

// Test String index property names
function testStr(o) {
    // Set 0, 1, 2
    guarded_call(function () {
        o[0] = "x";
        echo(" ", 0, o[0]);
    });
    guarded_call(function () {
        Object.defineProperty(o, 1, { value: "y" });
        echo(" ", 1, o[1]);
    });
    guarded_call(function () {
        Object.defineProperty(o, 2, { get: function () { return "z"; } });
        echo(" ", 2, o[2]);
    });

    // Set 6, 7
    guarded_call(function () {
        o[6] = "6";
        echo(" ", 6, o[6]);
    });
    guarded_call(function () {
        Object.defineProperty(o, 7, { get: function () { return "7"; }, enumerable: true });
        echo(" ", 7, o[7]);
    });

    guarded_call(function () {
        echo("  --- Properties ---");
        dumpProp(o);
    });
    guarded_call(function () {
        echo("  --- Enumerate ---");
        enumObj(o);
    });
}

scenario("Test String with String value", function() {
    testStr("abcd");
});

scenario("Test String with String object", function() {
    testStr(new String("abcd"));
});


scenario("Testing forin caching when forin changes from array to Es5array", function() {

var array = [4, 5, 6];
for (var i in array) {
    WScript.Echo(i);
}

// Adding a numeric property with attributes should create an ES5 array.
Object.defineProperty(array, "8", { get: function () { return 34; }, enumerable: true });

for (var i in array) {
    WScript.Echo(i);
}
}
);

scenario("Testing RegExp Number String Boolean Object Constructor length property attributes", function() {

function printAll()
{
    for(var i in arguments[0])
    {
        WScript.Echo(i + ":" + arguments[0][i]);
    }
}

printAll(Object.getOwnPropertyDescriptor(RegExp, "length"));
printAll(Object.getOwnPropertyDescriptor(String, "length"));
printAll(Object.getOwnPropertyDescriptor(Boolean, "length"));
printAll(Object.getOwnPropertyDescriptor(Number, "length"));
printAll(Object.getOwnPropertyDescriptor(Object, "length"));

});