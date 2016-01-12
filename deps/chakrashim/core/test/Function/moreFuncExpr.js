//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

function Test1()
{
    write("Decalaration Test1")
};

Test1();

var Result1 = function Test1()
{
    write("Expression Test1")
};

Test1();
Result1();



var Result2, Test2;
Result2 = function Test2(n)
{
    if (n < 0)
    {
        write("Test2: Less 0");
    }
    else
    {
        write("Test2: Greater 0");
        Test2(-n);
    }
}

Test2 = function Test2(n)
{
    write("In second Test2");
};

Result2(2); 


var fact, factorial;
fact = function factorial(n)
{
    return n<=1?1:n*factorial(n-1)
};

factorial = function factorial(n)
{
    return -1
};
write("Test3 factorial : " + fact (3)); 


function Test4()
{
    write("first declaration of Test4")
};

Test4();

function Test4()
{
    write("Second declaration of Test4")
};

Test4();


function Test5(n)
{
    return n<=1?1:n*Test5(n-1)
};

var Result5 = Test5;

Test5 = function (n)
{
    return -1
};

write("Test5 factorial : " + Result5(3)); 


var Test6 = function Test6()
{
    write(Test6)
};

var Result6 = Test6;

Test6 = "Outer Binding";

Result6();  
