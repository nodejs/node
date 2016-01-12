//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

var count = 0;
function check(scen) {
    var str = "check" + ++count + ": " + scen;
    try {
        var result = eval(scen);
        str += " #" + result;
    } catch (e) {
        str += " @" + e.message;
    }
    
    write(str);
}

var Test0 = function F_Test0()
{
}

var Test1 = function F_Test1()
{
    write(F_Test1);
}

// funcexpression assigned a value
var Test2 = function F_Test2()
{
    write(F_Test2);
    F_Test2 = 100;
    write(F_Test2);
}

//variable with same name
var Test3 = function F_Test3()
{
    write("Test3: " + F_Test3);
    var F_Test3;
    write("Test3: " + F_Test3);
}

// variable with same name with init
var Test4 = function F_Test4()
{
    write("Test4: " + F_Test4);
    var F_Test4 = 100;
    write("Test4: " + F_Test4);
}

// variable with same name with init
var Test5 = function F_Test5()
{
    write("Test5: " + F_Test5);
    var F_Test5;
    F_Test5 = 100;
    write("Test5: " + F_Test5);
}

// argument with same name
var Test6 = function F_Test6(F_Test6)
{
    write("Test6: " + F_Test6);
}

// argument with same name and assignment
var Test7 = function F_Test7(F_Test7)
{
    write("Test7: " + F_Test7);
    F_Test7 = 100;
    write("Test7: " + F_Test7);
}

// argument with same name and assignment
var Test8 = function F_Test8(F_Test8)
{
    write("Test8: " + F_Test8);
    F_Test8 = 100;
    write("Test8: " + F_Test8);
    write("Test8: " + arguments[0]);
}

// argument with same name and assignment
var Test9 = function F_Test9(F_Test9)
{
    write("Test9: " + F_Test9);
    arguments[0] = 100;
    write("Test9: " + F_Test9);
    write("Test9: " + arguments[0]);
}

eval("var Test10 = function F_Test10(){}");

var Test11 = function F_Test11()
{
    eval('write("Test11: " + F_Test11)');
}

eval("var Test12 = function F_Test12(){eval('write(\"Test12: \" + F_Test12)');}");

// funcexpression assigned a value
var Test13 = function F_Test13()
{
    write("Test13: " + F_Test13);
    eval("F_Test13 = 100");
    write("Test13: " + F_Test13);
}

// variable with same name with init
var Test14 = function F_Test14()
{
    write("Test14: " + F_Test14);
    eval("var F_Test14 = 100;");
    write("Test14: " + F_Test14);
}

// variable with same name with init
var Test15 = function F_Test15()
{
    write("Test15: " + F_Test15);
    eval("var F_Test15;");
    write("Test15: " + F_Test15);
    eval("F_Test15 = 100;");
    write("Test15: " + F_Test15);
}

// argument with same name
var Test16 = function F_Test16(F_Test16)
{
    eval("write(F_Test16)");
}

// argument with same name and assignment
var Test17 = function F_Test17(F_Test17)
{
    write("Test17: " + F_Test17);
    eval("F_Test17 = 100;");
    write("Test17: " + F_Test17);
}

// argument with same name and assignment
var Test18 = function F_Test18(F_Test18)
{
    write("Test18: " + F_Test18);
    eval("F_Test18 = 100;");
    write("Test18: " + F_Test18);
    write("Test18: " + arguments[0]);
}

// argument with same name and assignment
var Test19 = function F_Test19(F_Test19)
{
    write("Test19: " + F_Test19);
    eval("arguments[0] = 100;");
    write("Test19: " + F_Test19);
    write("Test19: " + arguments[0]);
}

var Test20 = function F_Test20()
{
  function inner20()
  {
      eval("var F_Test20 = 10");
      write(F_Test20);   
  } 
  inner20();
  WScript.Echo(F_Test20);
  return 0;
}

var Test21 = function F_Test21() 
{
    try {
        var x = function y() {
            var a = function b() {
                eval("WScript.Echo(y)");
                eval("y = 'legacy only'");
                eval("WScript.Echo(y)");
            }
            a();
        }
        x();
    } catch (ex) {
        WScript.Echo(ex);
    }
}

var Test22 = function F_Test22()
{
    eval('try {} catch(e) {} (function e(x){}); WScript.Echo(e);');
    return e;
}

var numTests = 23;

for (var i=0;i<numTests;i++)
{
    check("Test" + i + "();");
    check("Test" + i + "('hello');");
    check("F_Test" + i + "();");
}
