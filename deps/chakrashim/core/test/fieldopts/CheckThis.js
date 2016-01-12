//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Flavors of inlined LdThis, some of which will bail out if we force the optimization.

(function () {
    function f() {
        return this.foo();
    }

    var t = this;
    var x = { foo: function () { WScript.Echo(this); } };
    x.f = f;
    x.f();

    try {
        f();
    }
    catch (e) {
        WScript.Echo(e.message);
    }

    WScript.Echo(t === this);
})();

(function () {
    function f(o) {
        return o.foo();
    }

    var x = { foo: function () { WScript.Echo(this); } };
    f(x);
})();

function test() {
    Object.prototype['foo'] = function () {return this};
    var c = {}
    var x;
    x + c.foo("a");
    ((function(){
        return 1;
    })()).foo()
};

WScript.Echo(test());
WScript.Echo(test());
