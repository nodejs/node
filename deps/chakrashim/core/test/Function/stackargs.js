//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

    function write()
{
  WScript.Echo(arguments[0]);
}

write("Test case #1");
function TestCase1()
{
 function foo()
 {
   write(arguments.caller);
   write(arguments.callee);
 }
 foo();
}
TestCase1();

write("Test case #2");
function TestCase2()
{
 function foo()
 {
   var a = arguments;
   return a[0];
 }
 var j=0;
 for(var i=0;i<10;i++)
 {
   j +=foo(10);
 }
 write(j);
}
TestCase2();

write("Test case #3");
function TestCase3()
{
 function foo()
 {
   return arguments[1];  
 }
 write(""+foo(10));
}
TestCase3();

write("Test case #4");
function TestCase4()
{
 function foo()
 {
   var a = arguments;
   eval("arguments[1] = 10");
   return a[1];
 }
 write(foo());
}
TestCase4();

write("Test case #5");
function TestCase5()
{
 function foo()
 {
   for (var a = 1; a < arguments.length; ++a)
   {
     write(arguments[a]);
   }
 }
 foo(10,20,30,40);
}
TestCase5();

write("Test case #6");
function TestCase6()
{
 (function()
 {
   eval("write(arguments[0])");
 })("goodbye");
}
TestCase6();

write("Test case #7");
function TestCase7()
{
 function lenChange() 
 {
   write(arguments.length);
   arguments.length--;
  write(arguments.length);
 }
 lenChange(10,20,30);
}
TestCase7();

write("Test case #8");
function TestCase8()
{
 function foo()
 {
   try {
        arguments[1] = 26;
    } catch(e) 
    {
    }
   write(arguments[1]);
 }
 foo();
}
TestCase8();

write("Test case #9");
function TestCase9()
{
 function foo()
 {
   arguments.caller = 0;
 }
 foo();
}
TestCase9();

write("Test case #10");
function TestCase10()
{
 function foo()
 {
  write(typeof(arguments));
 }
 foo();
}
TestCase10();


write("Test case #11");
function TestCase11()
{
 function foo()
 {
   write(arguments.callee);
   write(arguments.caller);
 }
 function bar()
 { 
   foo();
 }
 bar();
}
TestCase11();

write("Test case #12");
function TestCase12()
{
 function foo()
 {
   function bar()
  {
    return arguments[0];
  }
  return bar(arguments[0]);
 }
 write(foo(10));
}
TestCase12();

write("Test case #13");
function TestCase13()
{
 (function ()
 { 
   write(arguments.callee);
 })();
}
TestCase13();

write("Test case #14");
function TestCase14()
{
 var escape;
 function foo()
 {
   escape = arguments.caller;
 }
 foo();
 write(escape);
}
TestCase14();

write("Test case #15");
function TestCase15()
{
  function foo()
  { 
    var i = "abc";
    write(arguments[i]);
  }
  foo();
}
TestCase15();

write("Test case #16");
function TestCase16()
{
  function foo()
  { 
    var a = []
    write(a[arguments]);
  }
  foo();
}
TestCase16();

write("Test case #17");
function TestCase17()
{
    (function () {
        write(arguments ^= 1);
    })();
    (function () {
        write(arguments *= 1.7);
    })();
}
TestCase17();

var print = function(){return this};


(function(){/*sStart*/;while(("u143C".x) && 0){print(functional >> arguments); };;/*sEnd*/})();

write("Test case #18");
// Check negative index
(function(){
    var i = -0x1000;
    (function(){
        write(arguments[i]);
    })();
})();

write("Test case #19");
(function(){
  function test0(){
  //Code Snippet: NegativeArgumentBug.ecs (Blue15423)
  for (var _i in arguments[1073741823]) { 
  };
  };
  test0(); 
})();


write("Test case #20");
(function(){
  var shouldBailout = false;
  function test() {
    var requif = function () {};
    var XPCSafeJSObjectWrapper = function () { };
    for (var e = (Math.atan(arguments) / new XPCSafeJSObjectWrapper(''))in requif.x) {
    };
  };
  test();
  test();
})();

write("Test case #21");
(function(){
  function foo(){
    foo.arguments[0] = 10;
    write(Math.max.apply(null, arguments));
  }
  foo(1);
  foo(1);
})();

write("Test case #22");
(function(){
    function test0(){
        var obj0 = {};
        // Snippets:caller2.ecs
        function v26969(){
            v26969.caller.arguments.length = 2;
            v26969.caller.arguments[1] = 40;
    
        }
        function v26970(v26971, v26972){
            write(v26971);
            write(v26972);
        }
        function v26976(){
            v26969();
            v26970.apply(obj0, arguments);
        }
        v26976(10);
        v26976(20);
        v26976(30);
    };

    // generate profile
    test0();

    // run JITted code
    test0();

    // run code with bailouts enabled
    shouldBailout = true;
    test0();
})()


// Various ways of calling/loading function with used/unused result.
write("Test case #23");
var bailout = false;
function test23()
{
  var a = [1,2,3];
  if (bailout)
  { 
     a = arguments;
  }
  return a[0];
}
write(test23([10,20,30]));
bailout = true;
write(test23([10,20,30]));

write("Test case #24");
bailout = false;
function test24()
{
  var a = arguments;
  if (bailout)
  { 
     a = [1,2,3];
  }
  return a[0];
}
write(test24([10,20,30]));
bailout = true;
write(test24([10,20,30]));

write("Test case #25");
bailout = false;
function test25()
{
  var a;
  if (bailout)
  { 
     a = arguments;
  }
  else
  {
     a = [1,2,3];
  }
  return a[0];
}
write(test25([10,20,30]));
bailout = true;
write(test25([10,20,30]));

write("Test case #26");
function test26()
{
  var args = arguments;
  if (args[1]) 
    args = [3,args[0], args[1]];
  else 
    args = [this, args[0]];

 return args[0];
}
write(test26(10,false));
write(test26(10,20,30));

write("Test case #27");
(function ()
{
  for(var x = arguments in "u6701") 
  {
  }
  write(x);
})();


write("Test case #28");
(function () {
    var XPCSafeJSObjectWrapper = function () {
    };
    (function () {
        if (!new XPCSafeJSObjectWrapper()) {
            var y, z = arguments, ksjxzy, w;
            throw -35184372088832;
        }
    write(z);
    }());
})();
