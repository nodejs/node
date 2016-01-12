//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var a = Object.create(Object.prototype, {
    foo: {
        configurable: true,
        get: function () {
            Object.defineProperty(this, "foo", {
                value: "new prop"
            });
            return "old prop";
        },
    }
});
var b = Object.create(a);
WScript.Echo(b.foo);
WScript.Echo(b.foo);

var c = Object.create(Object.prototype, {
    foo: {
        configurable: true,
        get: function () {
            Object.defineProperty(this, "foo", {
                value: "new prop"
            });
            return "old prop";
        },
    }
});
var d = Object.create(c);
d.x = 123;
WScript.Echo(d.foo);
WScript.Echo(d.foo);

var x = Object.create(Object.prototype, {
    foo: {
        configurable: true,
        get: function () {
            Object.defineProperty(y, "foo", {
                value: "new prop"
            });
            return "old prop";
        },
    }
});
var y = Object.create(x);
var z = Object.create(y);
WScript.Echo(z.foo);
WScript.Echo(z.foo);

var e = Object.create(Object.prototype, {
    foo: {
        configurable: true,
        get: function () {
            Object.defineProperty(this, "foo", {
                value: "new prop"
            });
            return "old prop";
        },
    }
});
WScript.Echo(e.foo);
WScript.Echo(e.foo);