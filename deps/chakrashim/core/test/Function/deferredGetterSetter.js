//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// /forcedeferparse test to make sure we can handle getters and setters at global scope,
// at function scope, and with nested functions.

var x = {
    _y : 'x.y',
    get y() { WScript.Echo('getting x.y'); return this._y; },
    set y(val) { WScript.Echo('setting x.y'); this._y = val; }
};

WScript.Echo(x.y);
x.y = 'new x.y';

function f() {

    var x = {
        _y : 'local x.y',
        get y() { WScript.Echo('getting local x.y'); return this._y; },
        set y(val) { WScript.Echo('setting local x.y'); this._y = val; }
    };

    WScript.Echo(x.y);
    x.y = 'new local x.y';

    var nested_x = {
        _y : 'nested x.y',
        get y() { function fget(o) { WScript.Echo('getting nested x.y'); return o._y; } return fget(this); },
        set y(val) { function fset(o, val) { WScript.Echo('setting nested x.y'); o._y = val; } fset(this, val); }
    };

    WScript.Echo(nested_x.y);
    nested_x.y = 'new nested x.y';
    WScript.Echo(nested_x.y);

    WScript.Echo(x.y);
}

f();

WScript.Echo(x.y);
