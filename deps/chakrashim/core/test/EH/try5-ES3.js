//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(args)
{
  WScript.Echo(args);
}

var scenario= 1;
function InitScenario()
{
  write("Scenario " + scenario++);
}

var e = "I am alive";

InitScenario(); //1

try
{
  throw "abc";
}
catch(e)
{
  write(e);
}
write(e);

InitScenario();//2

try
{
  throw "abc";
}
catch(e)
{
  e = 20;
  write(e);
}
write(e);


InitScenario();//3

var a = new Object();
a.e = "I am inside with";

with(a)
{
 try
 { 
   throw "abc";
 }
 catch(e)
 {
   e = 20;
   write(e);
 }
 write(e);
}
write(e);



InitScenario();//4

try
{ 
  throw "abc";
}
catch(e)
{
  var a = new Object();
  a.e = "I am inside with";

  with(a)
  {
    write(e);
  }
  write(e);
}
write(e);




InitScenario();//5

try
{ 
  throw "abc";
}
catch(e)
{
  var a = 10;
}
write(a);


InitScenario();//6
try
{ 
  throw "abc";
}
catch(e)
{
  var a = function () { return "hello world";};
}
write(a());

InitScenario();//7
try
{ 
  throw "abc";
}
catch(e)
{
  eval("a = function () { return 'hello world';};");
}
write(a());

InitScenario();//8
try
{ 
  throw "abc";
}
catch(e)
{
  c = 30;
}
write(c);



InitScenario();//9

var foo = function ()
{

    try
    {
      throw "abc";
    }
    catch(e)
    {
      write(e);
    }
    write(e);
}
foo();

InitScenario();//10

foo = function ()
{
    try
    {
      throw "abc";
    }
    catch(e)
    {
      e = 20;
      write(e);
    }
    write(e);
}
foo();

InitScenario();//11

foo = function ()
{
    var a = new Object();
    a.e = "I am inside with";

    with(a)
    {
     try
     { 
       throw "abc";
     }
     catch(e)
     {
       e = 20;
       write(e);
     }
     write(e);
    }
    write(e);
}
foo();

InitScenario();//12

foo = function ()
{
    try
    { 
      throw "abc";
    }
    catch(e)
    {
      var a = new Object();
      a.e = "I am inside with";

      with(a)
      {
        write(e);
      }
      write(e);
    }
    write(e);
}

InitScenario();//13

foo = function ()
{
    try
    { 
      throw "abc";
    }
    catch(e)
    {
      var a = 10;
    }
    write(a);
}
foo();


InitScenario();//14
foo = function ()
{
    try
    { 
      throw "abc";
    }
    catch(e)
    {
      var a = function () { return "hello world";};
    }
    write(a());
}
foo();

InitScenario();//15
foo = function ()
{
    try
    { 
      throw "abc";
    }
    catch(e)
    {
      eval("a = function () { return 'hello world';};");
    }
    write(a());
}
foo();

InitScenario();//16
foo = function ()
{
    try
    { 
      throw "abc";
    }
    catch(e)
    {
      c = 30;
    }
    write(c);
}
foo();

InitScenario();//17
foo = function ()
{
  var test = 'pass';
  try {
    throw 'fail';
  } catch (test) {
    test += 'ing';
  }
  write(test);
}
foo();


//raise bug for this eval scenario
InitScenario();
try
{ 
  throw "abc";
}
catch(e)
{
  eval("a = 10;");
}
write(a);


InitScenario();
foo = function ()
{
    try
    { 
      throw "abc";
    }
    catch(e)
    {
      eval("a = 19;");
    }
    write(a);
}
foo();


