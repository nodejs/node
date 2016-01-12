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

function dumpProp(obj, name) {
    var desc = Object.getOwnPropertyDescriptor(obj, name);
    var s = "  " + name + ": " + obj[name];
    if (desc) {
        if (desc.enumerable) { s += " enumerable"; }
        if (desc.configurable) { s += " configurable"; }
        if (desc.writable) { s += " writable"; }
        if (desc.getter) { s += " getter"; }
        if (desc.setter) { s += " setter"; }
    }
    echo(s);
}

scenario("[[Value]] absent, set enumerable: true", function () {
    var o = [];
    guarded_call(function () {
        Object.defineProperty(o, "length", { enumerable: true });
    });
    dumpProp(o, "length");
});

scenario("[[Value]] absent, set configurable: true", function () {
    var o = [];
    guarded_call(function () {
        Object.defineProperty(o, "length", { configurable: true });
    });
    dumpProp(o, "length");
});

scenario("[[Value]] absent, empty descriptor", function () {
    var o = [];
    Object.defineProperty(o, "length", {});
    dumpProp(o, "length");
});

scenario("[[Value]] absent, no change", function () {
    var o = [];
    Object.defineProperty(o, "length", { enumerable: false, configurable: false });
    dumpProp(o, "length");
});

scenario("[[Value]] absent, set writable: true", function () {
    var o = [];
    Object.defineProperty(o, "length", { writable: true });
    dumpProp(o, "length");
    o.length = 10;
    echo("  length:", o.length);
});

scenario("[[Value]] absent, set writable: false", function () {
    var o = [];
    Object.defineProperty(o, "length", { writable: false });
    dumpProp(o, "length");
    o.length = 10;
    echo("  length:", o.length);
});

scenario("[[Value]] absent, set multiple -- configurable: false, writable: false", function () {
    var o = [];
    guarded_call(function () {
        Object.defineProperty(o, "length", { configurable: false, writable: false });
    });
    dumpProp(o, "length");
});

scenario("[[Value]] absent, set multiple -- enumerable: false, writable: false", function () {
    var o = [];
    guarded_call(function () {
        Object.defineProperty(o, "length", { enumerable: false, writable: false });
    });
    dumpProp(o, "length");
});

scenario("[[Value]] invalid 1", function () {
    var o = [0, 1, 2];
    guarded_call(function () {
        Object.defineProperty(o, "length", { value: Infinity });
    });
    dumpProp(o, "length");
    echo(" ", o);
});

scenario("[[Value]] invalid 2", function () {
    var o = [0, 1, 2];
    guarded_call(function () {
        Object.defineProperty(o, "length", { value: -3 });
    });
    dumpProp(o, "length");
    echo(" ", o);
});

scenario("newLen >= oldLen, set enumerable: true", function () {
    var o = [0, 1, 2];
    guarded_call(function () {
        Object.defineProperty(o, "length", { value: 5, enumerable: true });
    });
    dumpProp(o, "length");
    echo(" ", o);
});

scenario("newLen >= oldLen, set enumerable: false", function () {
    var o = [0, 1, 2];
    Object.defineProperty(o, "length", { value: 5, enumerable: false });
    dumpProp(o, "length");
    echo(" ", o);
});

scenario("newLen >= oldLen, set configurable: true", function () {
    var o = [0, 1, 2];
    guarded_call(function () {
        Object.defineProperty(o, "length", { value: 5, configurable: true });
    });
    dumpProp(o, "length");
    echo(" ", o);
});

scenario("newLen >= oldLen, set configurable: false", function () {
    var o = [0, 1, 2];
    Object.defineProperty(o, "length", { value: 5, configurable: false });
    dumpProp(o, "length");
    echo(" ", o);
});

scenario("newLen >= oldLen, set configurable: false, enumerable: false", function () {
    var o = [0, 1, 2];
    Object.defineProperty(o, "length", { value: 5, configurable: false, enumerable: false });
    dumpProp(o, "length");
    echo(" ", o);
});

scenario("newLen >= oldLen, set configurable: false, enumerable: true", function () {
    var o = [0, 1, 2];
    Object.defineProperty(o, "length", { value: 5, configurable: false, enumerable: true });
    dumpProp(o, "length");
    echo(" ", o);
});

scenario("newLen >= oldLen, set writable: true", function () {
    var o = [0, 1, 2];
    Object.defineProperty(o, "length", { value: 5, writable: true });
    dumpProp(o, "length");
    echo(" ", o);
    o.length = 10;
    echo(" ", o);
});

scenario("newLen >= oldLen, set writable: false", function () {
    var o = [0, 1, 2];
    Object.defineProperty(o, "length", { value: 5, writable: false });
    dumpProp(o, "length");
    echo(" ", o);
    o.length = 10;
    echo(" ", o);
});

scenario("newLen < oldLen, oldLen writable: false", function () {
    var o = [0, 1, 2];
    Object.defineProperty(o, "length", { writable: false });

    guarded_call(function () {
        Object.defineProperty(o, "length", { value: 2, writable: true });
    });
    dumpProp(o, "length");
    echo(" ", o);
    o.length = 10;
    echo(" ", o);
});

scenario("newLen < oldLen, set enumerable: true", function () {
    var o = [0, 1, 2];
    guarded_call(function () {
        Object.defineProperty(o, "length", { value: 2, enumerable: true });
    });
    dumpProp(o, "length");
    echo(" ", o);
});

scenario("newLen < oldLen, set configurable: true", function () {
    var o = [0, 1, 2];
    guarded_call(function () {
        Object.defineProperty(o, "length", { value: 2, configurable: true });
    });
    dumpProp(o, "length");
    echo(" ", o);
});

scenario("newLen < oldLen, set writable: true", function () {
    var o = [0, 1, 2];
    Object.defineProperty(o, "length", { value: 2, writable: true });
    dumpProp(o, "length");
    echo(" ", o);
    o.length = 10;
    echo(" ", o);
});

scenario("newLen < oldLen, set writable: false", function () {
    var o = [0, 1, 2];
    Object.defineProperty(o, "length", { value: 2, writable: false });
    dumpProp(o, "length");
    echo(" ", o);
    o.length = 10;
    echo(" ", o);
});

scenario("newLen < oldLen, set writable: true, an element can't delete", function () {
    var o = [0, 1, 2, 3, 4, 5];
    Object.defineProperty(o, 3, { configurable: false });
    dumpProp(o, 3);

    guarded_call(function () {
        Object.defineProperty(o, "length", { value: 2, writable: true });
    });
    dumpProp(o, "length");
    echo(" ", o);
});

scenario("newLen < oldLen, set writable: false, an element can't delete", function () {
    var o = [0, 1, 2, 3, 4, 5];
    Object.defineProperty(o, 3, { configurable: false });
    dumpProp(o, 3);

    guarded_call(function () {
        Object.defineProperty(o, "length", { value: 2, writable: false });
    });
    dumpProp(o, "length");
    echo(" ", o);
});

scenario("newLen < oldLen, an accessor can't delete", function () {
    var o = [0, 1, 2, 3, 4, 5];
    Object.defineProperty(o, 3, { get: function(){ return "get 3"}, configurable: false });
    dumpProp(o, 3);

    guarded_call(function () {
        Object.defineProperty(o, "length", { value: 2, writable: false });
    });
    dumpProp(o, "length");
    echo(" ", o);
});

scenario("newLen < oldLen, an element can't delete, newLen == it", function () {
    var o = [0, 1, 2, 3, 4, 5];
    Object.defineProperty(o, 3, { configurable: false });
    dumpProp(o, 3);

    guarded_call(function () {
        Object.defineProperty(o, "length", { value: 3, writable: false });
    });
    dumpProp(o, "length");
    echo(" ", o);
});

scenario("newLen < oldLen, an element can't delete, but newLen beyond it", function () {
    var o = [0, 1, 2, 3, 4, 5];
    Object.defineProperty(o, 3, { configurable: false });
    dumpProp(o, 3);

    Object.defineProperty(o, "length", { value: 4, writable: false });
    dumpProp(o, "length");
    echo(" ", o);
});

scenario("newLen < oldLen, many attributes", function () {
    var o = [];
    for (var i = 0; i < 10; i++) {
        if (i % 2) {
            Object.defineProperty(o, i, { value: i, configurable: true });
        } else {
            var getValue = "get" + i;
            Object.defineProperty(o, i, { get: function () { return getValue; }, configurable: true });
        }
        dumpProp(o, i);
    }
    echo(" ", o);

    Object.defineProperty(o, "length", { value: 4, writable: false });
    dumpProp(o, "length");
    echo(" ", o);
    for (var i = 0; i < 10; i++) {
        dumpProp(o, i);
    }
});

scenario("newLen < oldLen, many attributes, one cannot delete", function () {
    var o = [];
    for (var i = 0; i < 10; i++) {
        if (i % 2) {
            var b = (i !== 5);
            Object.defineProperty(o, i, { value: i, configurable: b });
        } else {
            var getValue = "get" + i;
            Object.defineProperty(o, i, { get: function () { return getValue; }, configurable: true });
        }
        dumpProp(o, i);
    }
    echo(" ", o);

    guarded_call(function () {
        Object.defineProperty(o, "length", { value: 4, writable: false });
    });
    dumpProp(o, "length");
    echo(" ", o);
    for (var i = 0; i < 10; i++) {
        dumpProp(o, i);
    }
});

scenario("newLen < oldLen, many attributes, data item cannot delete", function () {
    var o = [];
    for (var i = 0; i < 10; i++) {
        if (i % 2) {
            Object.defineProperty(o, i, { value: i, configurable: true });
        } else {
            var getValue = "get" + i;
            Object.defineProperty(o, i, { get: function () { return getValue; }, configurable: true });
        }
    }
    // append some data item not in attribute map
    for (var i = 10; i < 15; i++) {
        o[i] = i;
    }
    Object.seal(o);
    echo(" ", o);

    guarded_call(function () {
        Object.defineProperty(o, "length", { value: 4, writable: false });
    });
    dumpProp(o, "length");
    echo(" ", o);
});

scenario("newLen < oldLen, a few attributes, one cannot delete", function () {
    var o = [];
    for (var i = 0; i < 10; i++) {
        if (i % 5) {
            o[i] = i;
        } else {
            Object.defineProperty(o, i, { value: i, configurable: false });
        }
    }
    echo(" ", o);

    guarded_call(function () {
        Object.defineProperty(o, "length", { value: 3, writable: false });
    });
    dumpProp(o, "length");
    echo(" ", o);
});

scenario("newLen < oldLen, no attributes, data item cannot delete", function () {
    var o = [];
    for (var i = 0; i < 10; i++) {
        o[i] = i;
    }
    Object.seal(o);
    echo(" ", o);

    guarded_call(function () {
        Object.defineProperty(o, "length", { value: 3, writable: false });
    });
    dumpProp(o, "length");
    echo(" ", o);
});

scenario("[[Put]] length: newLen < oldLen, many attributes", function () {
    var o = [];
    for (var i = 0; i < 10; i++) {
        if (i % 2) {
            Object.defineProperty(o, i, { value: i, configurable: true });
        } else {
            var getValue = "get" + i;
            Object.defineProperty(o, i, { get: function () { return getValue; }, configurable: true });
        }
        dumpProp(o, i);
    }
    echo(" ", o);

    o.length = 4;
    dumpProp(o, "length");
    echo(" ", o);
    for (var i = 0; i < 10; i++) {
        dumpProp(o, i);
    }
});

scenario("[[Put]] length: newLen < oldLen, many attributes, one cannot delete", function () {
    var o = [];
    for (var i = 0; i < 10; i++) {
        if (i % 2) {
            var b = (i !== 5);
            Object.defineProperty(o, i, { value: i, configurable: b });
        } else {
            var getValue = "get" + i;
            Object.defineProperty(o, i, { get: function () { return getValue; }, configurable: true });
        }
        dumpProp(o, i);
    }
    echo(" ", o);

    o.length = 4; // This would throw in strict mode
    dumpProp(o, "length");
    echo(" ", o);
    for (var i = 0; i < 10; i++) {
        dumpProp(o, i);
    }
});

scenario("Add item beyond non-writable length", function () {
    var o = [0, 1, 2, 3, 4, 5];
    Object.defineProperty(o, "length", { writable: false });

    guarded_call(function () {
        Object.defineProperty(o, 9, { value: 9, enumerable: true, configurable: true, writable: false });
    });
    echo(" ", o);
});

scenario("Add accessor beyond non-writable length", function () {
    var o = [0, 1, 2, 3, 4, 5];
    Object.defineProperty(o, "length", { writable: false });

    guarded_call(function () {
        Object.defineProperty(o, 9, { get: function () { return "get9"; }, configurable: true });
    });
    echo(" ", o);
});

scenario("SetItem beyond non-writable length", function () {
    var o = [0, 1, 2, 3, 4, 5];
    Object.defineProperty(o, "length", { writable: false });

    o[9] = 9; // This would throw in strict mode
    echo(" ", o);
});

scenario("SetItem with name beyond non-writable length", function () {
    var o = [0, 1, 2, 3, 4, 5];
    Object.defineProperty(o, "length", { writable: false });

    var name = "9";
    o[name] = 9; // This would throw in strict mode
    echo(" ", o);
});

scenario("freeze should set length writable: false", function () {
    var o = [0, 1, 2, 3, 4, 5];
    Object.freeze(o);
    dumpProp(o, "length");
});

scenario("isFrozen should check length writable", function () {
    var o = [0, 1, 2, 3, 4, 5];
    for (var i = 0; i < o.length; i++) {
        Object.defineProperty(o, i, { writable: false, configurable: false });
    }
    Object.preventExtensions(o);
    echo("isFrozen:", Object.isFrozen(o)); // false, because length writable

    Object.defineProperty(o, "length", { writable: false });
    echo("isFrozen:", Object.isFrozen(o));
});

