//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(value)
{
    WScript.Echo(value);
}

function RunTest(testCase, testCount)
{
  var testFunction = testCase[0];
  var testScenario = testCase[1];

  testScenario = " (test " + testCount + "): " + testScenario;

  write(testScenario);
  try
  {
    var result = testFunction();
    if (result == true)
    {
    write("PASS");
    }

  }
  catch (e)
  {
    var resultString = "FAILED" + testScenario;
    write(resultString + " :: " + e.message);
  }
}

function RunAllTests(){
  for(var i = 0; i < testList.length; ++i){
    RunTest(testList[i], i + 1);
  }
}

var testList = [

    [Test1,    "Object literal duplicate set/get property"],
    [Test2,    "Object literal set must have exactly one argument"],
];

// Utility functions
function Verify(expression, actualValue, expectedValue)
{
    if (!expectedValue) {
        if (actualValue) {
            write("Success: " + expression);
        } else {
            write("Failed: " + expression);
        }
        return actualValue;
    } else if (expectedValue != actualValue) {
        write("Failed: Expected " + expression + " = " + expectedValue + ", got " + actualValue);
        return false;
    }
  write("Success: Expected " + expression + " = " + expectedValue + ", got " + actualValue);
  return true;
}

//Tests

write("ES 5 Object Literal grammar: parsing-only tests");

// Test: Object literal duplicate set\get property. See ECMAScript 11.1.5.
function Test1(args)
{
   // duplicate property
   // This causes error only in the case when strict code mode is enabled. Strict mode is not implemented yet.
   var isOk = true;
   try
   {
     eval("function f() { var o = { foo: 1, foo: 2 }; }");
   }
   catch (ex)
   {
       isOk = Verify("duplicate data properties in object literal should not throw SyntaxError in non-strict mode", false);
   }

   // property and get
   try
   {
     eval("function f() { var o = { foo: 1, get foo() {return this.value;} }; }");
   }
   catch(ex)
   {
       isOk = Verify("data property and get with same name in object literal should not throw SyntaxError", false);
   }

   // get and property
   try
   {
     eval("function f() { var o = { get foo() {return this.value;}, foo: 1 }; }");
   }
   catch(ex)
   {
       isOk = Verify("get and data property with same name in object literal should not throw SyntaxError", false);
   }

   // property and set
   try
   {
     eval("function f() { var o = { foo: 1, set foo(arg) { this.value = arg;} }; }");
   }
   catch(ex)
   {
       isOk = Verify("data property and set with same name in object literal should not throw SyntaxError", false);
   }

   // set and property
   try
   {
     eval("function f() { var o = { set foo(arg) { this.value = arg;}, foo: 1 }; }");
   }
   catch(ex)
   {
       isOk = Verify("set and data property with same name in object literal should not throw SyntaxError", false);
   }

   try //duplicate get
   {
     eval("function f() { var o = {get foo(){return this.value;}, get foo(){return this.value*2;}}; }");
   }
   catch(ex)
   {
       isOk = Verify("duplicate get in object literal should not throw SyntaxError", false);
   }

   try //duplicate set
   {
     eval("function f() { var o = {set foo(args){this.value = args*2;}, set foo(args){ this.value = args*args}}; }");
   }
   catch(ex)
   {
       isOk = Verify("duplicate set in object literal should not throw SyntaxError", false);
   }

   try //duplicate get after get and set
   {
     eval("function f() { var o = { get foo(){ return this.value; }, set foo(arg){ this.value = arg; }, get foo(){ return this.value; } }; }");
   }
   catch(ex)
   {
       isOk = Verify("duplicate get after get and set in object literal should not throw SyntaxError", false);
   }

   try //duplicate set after set and get
   {
     eval("function f() { var o = { set foo(arg){ this.value = arg; }, get foo(){ return this.value; }, set foo(arg1){ this.value = arg1; } } }");
   }
   catch(ex)
   {
       isOk = Verify("duplicate set after get and set in object literal should not throw SyntaxError", false);
   }

   return isOk;
}

// Object literal set must have exactly one argument
function Test2()
{
   var exception;

   exception = null;
   try // setter with no arguments
   {
     eval("function f() { var o = { set foo() { this.value = 0; } } }");
   }
   catch (ex)
   {
     exception = ex;
   }
   if(!Verify("set with no arguments in object literal throws SyntaxError", true, exception instanceof SyntaxError)) return false;

   exception = null;
   try // setter with two arguments
   {
     eval("function f() { var o = { set foo(arg1, arg2) { this.value = 0; } } }");
   }
   catch (ex)
   {
     exception = ex;
   }
   if(!Verify("set with two arguments in object literal throws SyntaxError", true, exception instanceof SyntaxError)) return false;

   return true;
}

RunAllTests();

