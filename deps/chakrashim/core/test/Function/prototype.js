//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function echo(msg) {
    WScript.Echo(msg);
}

function guarded_call(func) {
    try {
        func();
    } catch (e) {
        echo(e.name + ": " + e.message);
    }
}

echo(typeof Function.prototype);
echo(Function.prototype);

echo(Function.prototype());
echo(Function.prototype("return 123;"));
echo(Function.prototype(123, "foo", null));

guarded_call(function () {
    var o = new Function.prototype();
    echo("Fail: " + o);
});

guarded_call(function () {
    var o = ({}) instanceof Function.prototype;
    echo("Fail: " + o);
});
