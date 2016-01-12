//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

function f() {
    write("this         : " + this);
    write("typeof(this) : " + typeof (this));
    eval('write("this         : " + this);');
    eval('write("typeof(this) : " + typeof (this));');
}

f();
f.call();
f.call(undefined);
f.call(null);
f.call(this);
f.call(10);
f.call(new Number(10));
