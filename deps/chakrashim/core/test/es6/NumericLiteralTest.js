//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
    {
        name: "Digit must follow leading 0<x|o|b> Numeric Literal Identifier Test",
        body: function ()
        {
            assert.throws(function () { eval("0b"); }, SyntaxError,"0b should not be valid");
            assert.throws(function () { eval("0o"); }, SyntaxError,"0o should not be valid");
            assert.throws(function () { eval("0x"); }, SyntaxError,"0x should not be valid");
        }
    },
    {
        name: "Octal Brute Force number Check",
        body: function ()
        {

            var o  = [0o0 ,0o1 ,0o2 ,0o3 ,0o4 ,0o5 ,0o6 ,0o7,0o10,
                      0o11,0o12,0o13,0o14,0o15,0o16,0o17
                     ];

            for(var i = 0; i < o.length;i++)
            {
                assert.areEqual(i,o[i],"decimal literal: ",i," should equal octal literal: ",o[i]);
            }
        }
    },
    {
        name: "Octal and Binary Number Constructor",
        body: function ()
        {
            assert.areEqual(1,  Number('0b1'));
            assert.areEqual(3,  Number('0b11'));
            assert.areEqual(NaN,Number('0b2'));
            assert.areEqual(1,  Number('0o1'));
            assert.areEqual(NaN,Number('0o9'));

        }
    },
    {
        name: "Octal Syntax Test (leading zeros, unexpected characters, & lower and upper case O",
        body: function ()
        {
            var o  = 0o765;
            var o1 = 0o102;
            var o2 = 0O543;
            var o3 = 0O000000000543;
            assert.throws(function () { eval("0o108"); }, SyntaxError,"an octal can't have an 8");
            assert.throws(function () { eval("0o10B"); }, SyntaxError,"an octal can't have a hex digit");
            assert.throws(function () { eval("0o109"); }, SyntaxError,"an octal can't have a 9");
            assert.areEqual(501, o,  "lower case O test");
            assert.areEqual(66,  o1, "lower case O test 2");
            assert.areEqual(355, o2, "upper case O test");
            assert.areEqual(355, o3, "leading 0 test");

        }
    },
    {
        name: "Octal Addition,Subtraction,Division, & Multiplication Test",
        body: function ()
        {
            var o1 = 0o10 + 0o6;
            var o2 = 0o12 - 0o5;
            var o3 = 0o17 * 0o2;
            var o4 = 0o22 / 0o11;
            var o5 = 0o13 / 0o10;
            assert.areEqual(14,   o1,"Addition Test");
            assert.areEqual(5,    o2,"Subtraction Test");
            assert.areEqual(30,   o3,"Multiplication Test");
            assert.areEqual(2,    o4,"simple division Test");
            assert.areEqual(1.375,o5,"Make sure decimal numbers work properly after division");
        }
    },
    {
        name: "Octal bitwise Operator Test",
        body: function ()
        {
            var o1 = 0o1 & 0o10;
            var o2 = 0o4 | 0o6;
            var o3 = 0o2 ^ 0o3;
            var o4 = 0o4 | 0o2;
            var o5 = 0o6 & 0o3;
            var o6 = 0o2 ^ 0o5;
            var o7 = ~0o10;
            assert.areEqual(0,o1, "bitwise And Test when no bit line up");
            assert.areEqual(6,o2, "bitwise Or Test when bits line up");
            assert.areEqual(1,o3, "bitwise Xor Test");
            assert.areEqual(6,o4, "bitwise Or when bits don't line up");
            assert.areEqual(2,o5, "bitwise And where a bit lines up");
            assert.areEqual(7,o6, "bitwise Xor when no bits line up");
            assert.areEqual(~8,o7,"bitwise Not");
        }
    },
    {
        name: "Octal +/- 0 is correct",
        body: function () {
            function isNegativeZero(x) { return x === 0 && (1 / x) === -Infinity; };

            assert.isFalse(isNegativeZero( 0o0), "Number.isInteger returns true for 0");
            assert.isTrue (isNegativeZero(-0o0), "Number.isInteger returns true for -0");
            assert.areEqual(isNegativeZero(0o0),isNegativeZero(+0o0), "should not be -0");
        }
    },
    {
        name: "Octal JSON parse",
        body: function () {
            var obj = [0o0, [0o0, [0o0, 0o1, 0o2, 0o3, 0o4, 0o5]], 0o2];
            function reviver(key, value)
            {
                if (key == "4") return undefined;
                if (key == "3") return "THREE";
                return value;
            }
            var str = JSON.stringify(obj);
            var parsedObj = JSON.parse(str, reviver);
            assert.areEqual(parsedObj, [0,[0,[0,1,2,"THREE",,5]],2], "JSON.parse() should assign the value returned by reviver for array");
            var str = "[0o0, [0o0, [0o0, 0o1, 0o2, 0o3, 0o4, 0o5]], 0o2]";
            assert.throws(function () { var parsedObj = JSON.parse(str); }, SyntaxError,"JSON should not support Octal literals");
        }
    },

    {
        name: "Binary Brute Force number Check",
        body: function ()
        {

            var b  = [0b0,0b1,0b10,0b11,0b100,0b101,0b110,0b111,0b1000,
                      0b1001,0b1010,0b1011,0b1100,0b1101,0b1110,0b1111,
                     ];

            for(var i = 0; i < b.length;i++)
            {
                assert.areEqual(i,b[i],"decimal literal: ",i," should equal binary literal: ",b[i]);
            }
        }
    },
    {
        name: "Binary Front zeros, 0B format Tests, and unexpected characters",
        body: function ()
        {
            var b1 = 0b000001;
            var b2 = 0B00101;

            assert.throws(function () { eval("0b102"); }, SyntaxError,"a binary number can't have any digit greater than 1");
            assert.throws(function () { eval("0b10B"); }, SyntaxError,"a binary number can't have a hex digit");
            assert.areEqual(1,b1,"front zero test");
            assert.areEqual(5,b2,"upper case B test");
        }
    },
    {
        name: "Binary Addition,Subtraction,Division, & Multiplication Test",
        body: function ()
        {
            var b1 = 0b000001 + 0B1010;
            var b2 = 0B00101  - 0b100;
            var b3 = 0b10     * 0b11;
            var b4 = 0b100    / 0b10;
            var b5 = 0b1011   / 0b1000;
            assert.areEqual(11,    b1,"Addition Test");
            assert.areEqual(1,     b2,"Subtraction Test");
            assert.areEqual(6,     b3,"Multiplication Test");
            assert.areEqual(2,     b4,"Simple Division Test");
            assert.areEqual(1.375, b5,"Make sure decimal numbers work properly after division");
        }
    },
    {
        name: "Binary bitwise Operator Test",
        body: function ()
        {
            var b1 = 0b000001 & 0B1010;
            var b2 = 0B00101  | 0b100;
            var b3 = 0b10     ^ 0b11;
            var b4 = 0b100    | 0b10;
            var b5 = 0b110    & 0b11;
            var b6 = 0b10     ^ 0b101;
            var b7 = ~0b10;
            assert.areEqual(0,b1, "bitwise And Test when no bit line up");
            assert.areEqual(5,b2, "bitwise Or Test when bits line up");
            assert.areEqual(1,b3, "bitwise Xor Test");
            assert.areEqual(6,b4, "bitwise Or when bits don't line up");
            assert.areEqual(2,b5, "bitwise And where a bit lines up");
            assert.areEqual(7,b6, "bitwise Xor when no bits line up");
            assert.areEqual(~2,b7,"bitwise Not");
        }
    },
    {
        name: "Binary Max number and Proper rounding Tests",
        body: function()
        {
            //signed 32 bit int max
            var b1 = 0b01111111111111111111111111111111;
            assert.areEqual(2147483647,b1);//does b1 = 2^31-1?
            var b2 = 0b0111111111111111111111111111111100000000001111111111; //51 bits
            var b3 = 0b01111111111111111111111111111111000000000011111111110;//52 bits
            var b4 = 0b100000000000000000000000000000000000000000000000000000;//53 bits (first unsafe number)
            var b5 = 0b100000000000000000000000000000000000000000000000000001;//53 bits (unsafe number)
            var b6 = 0b100000000000000000000000000000000000000000000000000101;
            var b7 = 0b0111111111111111111111111111111111111111111111111111110;//54 bits
            assert.areEqual(Number.MAX_SAFE_INTEGER, 0b011111111111111111111111111111111111111111111111111111,
            "Number.MAX_SAFE_INTEGER is the largest integer value representable by Number without losing precision, i.e. 9007199254740991");
            assert.areEqual(2251799812637695,b2,"Test to stress the less than or equal to 52 bit conditional case failed");
            assert.areEqual(4503599625275390,b3,"Test to stress the 53rd bit conditional case in javahelp.cpp failed");
            assert.areEqual(9007199254740992,b4,"should not have to round up or down");
            assert.areEqual(9007199254740992,b5,"should round down from 9007199254740993 by 1");
            assert.areEqual(9007199254740996,b6,"should round up from 9007199254740995 by 1 to");
            assert.areEqual(18014398509481982,b7,"1 bit out of precison range Test");




        }
    },
    {
        name: "Binary +/- 0 is correct",
        body: function () {
            function isNegativeZero(x) { return x === 0 && (1 / x) === -Infinity; };

            assert.isFalse(isNegativeZero( 0b0), "Number.isInteger returns true for 0");
            assert.isTrue (isNegativeZero(-0b0), "Number.isInteger returns true for -0");
            assert.areEqual(isNegativeZero(0b0),isNegativeZero(+0b0), "should not be -0");
        }
    },
    {
        name: "Binary JSON parse",
        body: function () {
            var obj = [0b0, [0b0, [0b0, 0b1, 0b10, 0b11, 0b100, 0b101]], 0b10];
            function reviver(key, value)
            {
                if (key == "4") return undefined;
                if (key == "3") return "THREE";
                return value;
            }
            var str = JSON.stringify(obj);
            var parsedObj = JSON.parse(str, reviver);
            assert.areEqual(parsedObj, [0,[0,[0,1,2,"THREE",,5]],2], "JSON.parse() should assign the value returned by reviver for array");
            var str = "[0b0, [0b0, [0b0, 0b1, 0b10, 0b11, 0b100, 0b101]], 0b10]";
            assert.throws(function () { var parsedObj = JSON.parse(str); }, SyntaxError,"JSON should not support Binary literals");
        }
    }];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
