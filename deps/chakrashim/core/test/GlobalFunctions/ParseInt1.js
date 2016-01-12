//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

//Parse Int Test cases
var id=0;
function verify(get_actual,get_expected,id,testdesc)
{
    if(isNaN(get_actual) && isNaN(get_expected))
    {
        return;
    }
    if(get_actual !==get_expected)
            WScript.Echo(id+":"+testdesc+"\t"+"failed Actual:" + get_actual + " Excpected:" + get_expected);
}

//White space at the begining

verify(parseInt("     123",10),123,id++,"\"Testing WhiteSpace at the begining of the string\"")

//Escape characters at the begining

verify(parseInt("\t\n\f\r123",10),123,id++,"\"Testing WhiteSpace with escape at the begining of the string\"")

// only white spaces

verify(parseInt("\t\n\f\r",10), NaN,id++,"\"Only whitespaces\"")

// embedded null
verify(parseInt("32\032",10), 32,id++,"\"Embedded null\"")

//White Spaces in the End

verify(parseInt("123     ",10),123,id++,"\"Testing WhiteSpace at the End of the string\"")

//Escape characters at the end

verify(parseInt("123\t\n\f\r",10),123,id++,"\"Testing WhiteSpace with escape at the end of the string\"")

//Space in between Strings

verify(parseInt("12  3",10),12,id++,"\"Testing WhiteSpace in between strings\"")

//Escape Characters in between Strings

verify(parseInt("12\n\t\r\f3",10),12,id++,"\"Testing Escape Characters in between of the strings\"")

//Default Radix Testing: Null

verify(parseInt("123",null),123,id++,"\"Default Radix Null\"")

//Default Radix Testing: NaN

verify(parseInt("123",NaN),123,id++,"\"Default Radix NaN\"")

//Default Radix Testing: Undefined

verify(parseInt("123",undefined),123,id++,"\"Default Radix Undefined\"")

//Default Radix Testing: 0

verify(parseInt("123",0),123,id++,"\"Default Radix 0\"")

//Default Radix Testing with 0X String :Basic

verify(parseInt("0x19",16),25,id++,"\"Default Radix With 0x String: Basic\"")

//Default Radix Testing With 0x String: Null

verify(parseInt("0xFF",null),255,id++,"\"Default Radix With 0x String: Null\"")

//Default Radix Testing With 0x: NaN

verify(parseInt("0xFF",NaN),255,id++,"\"Default Radix With 0x String:NaN\"")

//Default Radix Testing With 0x: Undefined

verify(parseInt("0xFF",undefined),255,id++,"\"Default Radix With 0x String: undefined\"")

//Default Radix Testing With 0x: 0

verify(parseInt("0xFF",0),255,id++,"\"Default Radix With 0x String: 0\"")

//Default Radix Testing With 0X String: Null

verify(parseInt("0XFF",null),255,id++,"\"Default Radix With 0X String: Null\"")

//Default Radix Testing With 0X: NaN

verify(parseInt("0XFF",NaN),255,id++,"\"Default Radix With 0X String:NaN\"")

//Default Radix Testing With 0X: Undefined

verify(parseInt("0XFF",undefined),255,id++,"\"Default Radix With 0X String: undefined\"")

//Default Radix Testing With 0X: 0

verify(parseInt("0XFF",0),255,id++,"\"Default Radix With 0X String: 0\"")

//Default Radix Negative Testing: Out of bound Strings: Null

var x=parseInt("A123",null)

verify(isNaN(x),true,id++,"\"Default Radix:null  Negative Testing Null\"")

//Default Radix Negative Testing : Out of bound Strings: NaN

var x=parseInt("A123",NaN)

verify(isNaN(x),true,id++,"\"Default Radix Negative Testing NaN\"")

//Default Radix Negative Testing : Out of bound Strings: undefined

var x=parseInt("A123",undefined)

verify(isNaN(x),true,id++,"\"Default Radix Negative Testing undefined\"")

//Default Radix Negative Testing : Out of bound Strings: 0

var x=parseInt("A123",0)

verify(isNaN(x),true,id++,"\"Default Radix Negative Testing 0\"")

//Default Radix Negative Testing: Out of bound Strings with 0x: Null

var x=parseInt("0xG123",null)

verify(isNaN(x),true,id++,"\"Default Radix with 0x String  Negative Testing Null\"")

//Default Radix Negative Testing: Out of bound Strings with 0x: NaN

var x=parseInt("0xG123",NaN)

verify(isNaN(x),true,id++,"\"Default Radix with 0x String  Negative Testing NaN\"")

//Default Radix Negative Testing: Out of bound Strings with 0x: undefined

var x=parseInt("0xG123",undefined)

verify(isNaN(x),true,id++,"\"Default Radix with 0x String  Negative Testing undefined\"")

//Default Radix Negative Testing: Out of bound Strings with 0x: 0

var x=parseInt("0xG123",0)

verify(isNaN(x),true,id++,"\"Default Radix with 0x String  Negative Testing 0\"")

//Default Radix Negative Testing: Out of bound Strings with 0X: Null

var x=parseInt("0XG123",null)

verify(isNaN(x),true,id++,"\"Default Radix with 0X String  Negative Testing Null\"")

//Default Radix Negative Testing: Out of bound Strings with 0X: NaN

var x=parseInt("0XG123",NaN)

verify(isNaN(x),true,id++,"\"Default Radix with 0X String  Negative Testing NaN\"")

//Default Radix Negative Testing: Out of bound Strings with 0X: undefined

var x=parseInt("0XG123",undefined)

verify(isNaN(x),true,id++,"\"Default Radix with 0X String  Negative Testing undefined\"")

//Default Radix Negative Testing: Out of bound Strings with 0X: 0

var x=parseInt("0XG123",0)

verify(isNaN(x),true,id++,"\"Default Radix with 0X String  Negative Testing 0\"")

//Radix Testing Limits: Lower Limit 2

verify(parseInt("101",2),5,id++,"\"Radix Testing Limits: Lower Limit 2\"");

//Radix Testing Limits: upper limit 36

verify(parseInt("aAzZ",36),480815,id++,"\"Radix Testing Limits: Upper Limit 36\"");

//Radix Testing Limits:  +0

verify(parseInt("11",+0),11,id++,"\"Radix Testing Limits: +0\"");

//Radix Testing Limits:  -0

verify(parseInt("11",-0),11,id++,"\"Radix Testing Limits: -0\"");

//Radix Testing Limits:  -0.0

verify(parseInt("11",-0.0),11,id++,"\"Radix Testing Limits: -0.0\"");

//Radix Testing Limits:  Infinity

verify(parseInt("11",Infinity),11,id++,"\"Radix Testing Limits: Infinity\"");

//Radix Testing Limits: check for 1

var x=parseInt("10",1)

verify(isNaN(x),true,id++,"\"Radix Testing Limits check for 1\"")

//Radix Testing Limits: check for -1

var x=parseInt("10",-1)

verify(isNaN(x),true,id++,"\"Radix Testing Limits check for -1\"")

//Radix Testing Limits: check for 37

var x=parseInt("10",37)

verify(isNaN(x),true,id++,"\"Radix Testing Limits check for 37\"")

//Radix Testing : Non Integer: String

verify(parseInt("11","+2"),3,id++,"\"Radix Testing : Non Integer: String \"")

//Radix Testing : Non Integer : Boolean: true is replaced with a 1

var x=parseInt("10",true)

verify(isNaN(x),true,id++,"\"Radix Testing : Non Integer : Boolean:true is replaced with a 1\"")

//Radix Testing : Non Integer : Boolean: false is replaced with a 0

verify(parseInt("11",false),11,id++,"\"Radix Testing : Non Integer : Boolean: false is replaced with a 0 \"")

//Radix Testing : Output from a Constructor: Number

verify(parseInt("A",new Number(16)),10,id++,"\"Radix testing: Constructor-Number \"");

//Radix Testing Output from a constructor : String

verify(parseInt("A",new String("16")),10,id++,"\"Radix testing: Constructor-String \"");

//Radix Testing Output from a variable

var obj=36

verify(parseInt("aAzZ",obj.toString()),480815,id++,"\"Radix testing: Variable \"");

//Radix Testing Function

function fun()
{
    return "35"
}

verify(parseInt("bY",fun()),419,id++,"\"Radix testing:Function \"");

//String Testing : Null

verify(isNaN(parseInt("",10)),true,id++,"\"String Testing :null\"");

//String Testing 2^32

verify(parseInt("4294967296",10),4294967296,id++,"\"String Testing :2^32 \"");

//String Testing -2^32

verify(parseInt("-4294967296",10),-4294967296,id++,"\"String Testing :2^32 \"");

verify(parseInt("999999999",10),999999999,id++,"\"Large int :999999999 \"");

verify(parseInt("-FFFFFFFF",16),-0xFFFFFFFF,id++,"\"Max Neg int (Base 16) :FFFFFFFF \"");

verify(parseInt("-0xFFFFFFFF",16),-0xFFFFFFFF,id++,"\"Max Neg int (Base 16) :FFFFFFFF \"");

verify(parseInt("-0xABCDEF",16),-0xabcdef,id++,"\"Base 16 number\"");

verify(parseInt("-0xabcdef",16),-0xabcdef,id++,"\"Base 16 number\"");

verify(parseInt("abcdefghijklm",34), 24661871785383067000,id++,"\" Base 34 number \"");

verify(parseInt("lmnXYZ",36), 1307858363,id++,"\"Base 36 number - fast path \"");

verify(parseInt("lmnXYZabc",36), 61019439797496,id++,"\"Base 36 number - slow path \"");
//String Testing : Unmatched numbers for the radix

verify(parseInt("AB",11),10,id++,"\"String Testing: unmatched numbers for radix\"");

//String Testing :Expressions

verify(parseInt("A+5",11),10,id++,"\"String Testing: Expressions\"");

//String Testing : floating point

verify(parseInt("5.67",10),5,id++,"\"String Testing: Floating point numbers\"");

//String Testing : Octal Numbers

verify(parseInt("00789",008),7,id++,"\"String Testing: Octal Numbers\"");

//Substring scenarios
var strings = [
    { str: "+0x32", start: 0, length: 1, expected: NaN, expectedBase10: NaN },
    { str: "+0x32", start: 0, length: 1, expected: NaN, expectedBase10: NaN },
    { str: "+0x32", start: 0, length: 2, expected: 0, expectedBase10:0  },
    { str: "+0x32", start: 0, length: 3, expected: NaN, expectedBase10: 0 },
    { str: "+0x32", start: 0, length: 4, expected: 3, expectedBase10:0 }
    ];

for(var i =0; i < strings.length; i++)
{
    var current = strings[i];
    var substr = current.str.substring(current.start, current.length);
    verify(parseInt(substr), current.expected, id++, "Substring testing base: implicit string:" + substr);
    verify(parseInt(substr, 10), current.expectedBase10, id++,  "Substring testing base: 10 string:" + substr);
    verify(parseInt(substr, 16), current.expected, id++, "Substring testing base: 16 string:" + substr);
}

WScript.Echo("Done")
