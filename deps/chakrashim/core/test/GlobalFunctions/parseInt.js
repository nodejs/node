//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function verify(x,y)
{
    if(x != y)
    {
        WScript.Echo("ERROR: " + x + " != " + y );
    }
    else
    {
        WScript.Echo(x + " == " + y);
    }
}

WScript.Echo(parseInt());
WScript.Echo(parseInt(""));
WScript.Echo(parseInt("a"));
WScript.Echo(parseInt("0x"));
WScript.Echo(parseInt("0xz"));
WScript.Echo(parseInt(2, 2));
WScript.Echo(parseInt(2,"blah"));
WScript.Echo(parseInt("e10"));
WScript.Echo(parseInt("100", 37));
WScript.Echo(parseInt("100", 1));
WScript.Echo(parseInt("100", -1));
WScript.Echo(parseInt("100", -1000));
WScript.Echo(parseInt("100", 1000));

verify(0, parseInt("0"));
verify(0, parseInt("0z"));
verify(0, parseInt("-0"));
verify(0, parseInt("-0z"));
verify(0, parseInt(0));
verify(0, parseInt(-0)); // -0 to "0" to 0

// Verify again with 1 / 0 because 0 === -0 and we want to make sure that the sign is correct
verify(1 / 0, 1 / parseInt("0"));
verify(1 / -0, 1 / parseInt("-0"));
verify(1 / 0, 1 / parseInt(0));
verify(1 / 0, 1 / parseInt(-0)); // 1 / (-0 to "0" to 0) == +Infinity

verify(1, parseInt("1"));
verify(-1, parseInt("-1"));
verify(-1, parseInt(" -1"));
verify(-1, parseInt("\r\n\t\r\n    \r\n\t-1"));
verify(2, parseInt("2"));
verify(-3, parseInt("-3"));

verify(536870911, parseInt("536870911"));
verify(536870912, parseInt("536870912"));
verify(-536870912, parseInt("-536870912"));
verify(-536870913, parseInt("-536870913"));

verify(0x7fffffff, parseInt("2147483647"));
verify(-0x80000000, parseInt("-2147483648"));

verify(0x15, parseInt("0x15"));
verify(0x15, parseInt("0X15"));

verify(12, parseInt("12.5"));

verify(128, parseInt("10000000", 2));
verify(480815, parseInt("aAzZ", 36));
verify(480815, parseInt("aAzZ", "  00036"));
verify(13929, parseInt("bcY", new Number(35)));
verify(16, parseInt("g", new String("17")));

verify(0, parseInt("08")); // classic behavior
verify(8, parseInt("010")); // classic behavior
verify(8, parseInt(08));
verify(8, parseInt(010));

verify(0, parseInt("0x123", "10"));
verify(291, parseInt("0x123"));

verify(12, parseInt(new String("12")));
verify(-12, parseInt(new Number(-12)));

function isNegZero(n)
{
    return ((n === 0) && (1/n === -Infinity))
}
var i = 0;
var ni = -i;
WScript.Echo(isNegZero(ni));
WScript.Echo(isNegZero(parseInt(ni)));
