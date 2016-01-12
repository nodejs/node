//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

try
{
    x = random();
}
catch ( e )
{
    write(e.number + " " + e.message);
}

try
{
    throwException();
}
catch ( e )
{
    write(e.number + " " + e.message);
}

try {
    var astr = new AString();
}
catch(e) {
    write(e.number + " " + e.message);
}

try
{
    eval("function u\u3000n01() { return 3; }");
}
catch ( e )
{
    write(e.number + " " + e.message);
}

try
{
    var d = new Date();
    d.setHours();
}
catch ( e )
{
    write(e.number + " " + e.message);
}

try
{
    sTmp = encodeURI(String.fromCharCode(0xD800));
}
catch ( e )
{
    write(e.number + " " + e.message);
}

try
{
    sTmp = decodeURI("%");
}
catch ( e )
{
    write(e.number + " " + e.message);
}

try
{
    var data = "AABBCCDD";
    var exp = new RegExp("(?{ $a = 3+$b })");
    res = data.match(exp);
}
catch (e)
{
    write(e.number + " " + e.message);
}

try
{
    var data = "foo";
    var exp = new RegExp("(in","i");
    res = data.match(exp);
}
catch (e)
{
    write(e.number + " " + e.message);
}

try
{
    var numvar = new Number(10.12345);
    var res = numvar.toPrecision(0);
}
catch (e)
{
    write(e.number + " " + e.message);
}

try
{
    var exp = new RegExp("[z-a]","i");
}
catch (e)
{
    write(e.number + " " + e.message);
}

try
{
    eval("var u\u200Cn01 = 14;");
}
catch (e)
{
    write(e.number + " " + e.message);
}

