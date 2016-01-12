//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write()
{
  for(var i=0;i<arguments.length;i++)
  {
    WScript.Echo(arguments[i]);
  }
}

function TrimStackTracePath(line) {
    return line && line.replace(/\(.+\\test.bugs./ig, "(");
}

write("For Win8 934770");
write("Test case 1");

write((function(){
  var a;
  a <<=1;
  a = (1 <= a);
  return a;
})());

write("Test case 2");

write((function(){
  var a;
  a <<=1;
  a = (1 < a);
  return a;
})());

write("Test case 3");

write((function(){
  var a;
  a <<=1;
  a = (1 == a);
  return a;
})());

write("Test case 4");

write((function(){
  var a;
  a <<=1;
  a = (1 != a);
  return a;
})());

write("Test case 5");

write((function(){
  var a;
  a <<=1;
  a = (1 > a);
  return a;
})());

write("Test case 6");

write((function(){
  var a;
  a <<=1;
  a = (1 >= a);
  return a;
})());

write("Test case 7");

write((function(){
  var a;
  a <<=1;
  a = (1 !== a);
  return a;
})());

write("Test case 8");

write((function(){
  var a;
  a <<=1;
  a = (1 === a);
  return a;
})());

write("Test case 9");

write((function (b){
  var a;
  a <<=1;
  a = (a <= b);
  return a;
})(1));

write("Test case 10");

write((function (b){
  var a;
  a <<=1;
  a = (a < b);
  return a;
})(1));

write("Test case 11");

write((function (b){
  var a;
  a <<=1;
  a = (a == b);
  return a;
})(1));

write("Test case 12");

write((function (b){
  var a;
  a <<=1;
  a = (a != b);
  return a;
})(1));

write("Test case 13");

write((function (b){
  var a;
  a <<=1;
  a = (a === b);
  return a;
})(1));

write("Test case 14");

write((function (b){
  var a;
  a <<=1;
  a = (a !== b);
  return a;
})(1));

write("Test case 15");

write((function (b){
  var a;
  a <<=1;
  a = (a >= b);
  return a;
})(1));

write("Test case 16");

write((function (b){
  var a;
  a <<=1;
  a = (a > b);
  return a;
})(1));

write("Test case 17");

write((function (a){
  a = (a != a);
  return a;
})(1));

write("Test case 18");

write((function (a){
  a = (a === a);
  return a;
})(1));

write("Win 8 935276");

write("Test case 19");
write((function (p2,p3){
  return (((p2 = p3* 3) + p2));
})(10,20));

write("Test case 20");
write((function (p2,p3){
  return (((p2 = p3 * 3) + (p2 = p3 *4) ));
})(46,2));

write("Test case 21");
write((function (p2,p3){
  return (((p2 = p3 * -8323432) + p3 ));
})(44,23));

write("Test case 22");
write((function (p2,p3){
  return ((p3 + (p3 = p2 * p2) ));
})(-46,-20));

write("Test case 23");
write((function (p2,p3){
  return (((p3 = p2 * p3) + p3 ));
})(-23,20));

write("Test case 24");
write((function (p2,p3){
  return (((p3 = p2 * p3 * 4) + p3 ));
})(10,20));

write("Test case 25");
write((function (p2,p3){
  return (((p3 = p2 * 23) + p3 ));
})(10, new Number(-234)));

write("Test case 26");
write((function (p2,p3,p4){
  return (((p3 = p2 * p2) + p3 + (p4=p3*p2) ));
})(10, 20, 30));

write("Test case 27");
function test27(){
  var obj0 = {};
  var arrObj0 = {};
  var ui32 = new Uint32Array(256);
  var c = 1;
  obj0.prop0 = 1;
  function bar1 (){
  }
  if(ui32[1] == 1 > 1) {
    if((new bar1()).prop0 ) {
    }
    else {
      c=arrObj0.prop0;
    }
  }
};
// generate profile
test27();
write("Passed");

write("Test case 28");
function test28(a){
   a=String.fromCharCode(a);
}
test28(10);
write("Passed");

write("Test case 29");
function test29helper(_array2tmp) {
    for(var i in _array2tmp)
     {
      write(i);
     }
 }

function test29(){
  var func0 = function(){
    test29helper([h]);
  }
  var h = -2147483648;
  func0();
  ++h;
  ++h; //creates a missing value
  func0();
};
// generate profile
test29();

write("Test case 30");
function test30(){
  var floatary = [-1.5];
  if(floatary.length) {
  }
  else {
    // Array expression
    var _array1 = [(-1 * -1 - 2147483647)];

  }
};
test30();
write("Passed");

write("Test case 31");
(function test31()
{
    var func2 = function()
    {
        throw new Error();
    }
    function testlinenumber()
    {
        var arrObj0 = {};
    arrObj0.prop1 = 1;
    (1 ? 1 : 1) >= func2();
    };
    try
    {
    testlinenumber();
    }
    catch(ex)
    {
        write(TrimStackTracePath(ex.stack));
    }
})();
write("Passed");

write("Test case 32");
(function test32()
{
    var shouldBailout = false;
    function test0()
    {
       var arrObj0 = {};
       var func1 = function(){
       var __loopvar4 = 0;
       for(var strvar0 in i32 ) {
          if(strvar0.indexOf('method') != -1) continue;
          if(__loopvar4++ > 3) break;
          arrObj0.length =1;
          continue ;
          ary0 = arguments;
       }
     }
     Object.prototype.method0 = func1;
     var i32 = new Int32Array(1);
     var e = 1;
     e &=(shouldBailout ? (Object.defineProperty(arrObj0, 'length', {set: function(_x) { write('arrObj0.length setter'); }, configurable: true}), arrObj0.method0()) : arrObj0.method0());
    };

    // generate profile
    test0();
    shouldBailout = true;
    test0();
})();
write("Passed");

write("Test case 33");
(function test33()
{
    try
    {
        function inlinee(arg0 , arg1 , arg2)
        {
            throw new Object();
        }
        function inliner(arg0 , arg1)
        {
        }
        function func()
        {
            inliner(29,39,inlinee(22,33,44,55));
        }
        func(24,42);
    }catch(e){};
})();
write("Passed");

write("Test case 34");
(function test34()
{
    var a;
    a = (typeof(a) == "boolean");
    write(a);
})();
write("Passed");

write("Test case 34");
(function test34()
{
    for (var x = 1; x >= 0; x--)
    {
        var f = [];
        var c = f[0] ;
        c = f.push(c);
        write(f[0]);
    }
})();
write("Passed");

write("Test case 35")
function test35()
{
   if(typeof EvalError == "test") //use random comparsion
   {
     return true;
   }
   return false;
}
test35();
write("Passed");

write("Test case 36")
function test36() {
    (function () {
        for (let hvkbnr in null)
            throw 'u5623';
    }());
}
try
{
 test36();
}catch(e)
{
}
try
{
 test36();
}catch(e)
{
}
write("Passed")

write("Test case 37")
var test37 = function()
{
};
test37.prototype.B = function(a,b,c,d){return  a+b+c+d;};
var A = new test37();

function F()
{
 this.init.apply(this,arguments);
}
F.prototype.init = function()
{
  A.B.apply(this, arguments);
}
function bar()
{
  return new F(10,30,40,50);
}
write(bar());
write(bar());
write("passed");

write("Test case 38")
var test38 = function (d, j, a)
{
    do
    {
        if (d >= j)
        {
           break;
        }
    }
    while(1);
    for (;d < j;)
    {
    }
    return 10;
};
write("passed")

write("Test case 40");
(function test31()
{
    function testRuntimeError()
    {
        eval(" for (var x in []) { undefinedFunction((test ? false &= 1 : true)); }");
    };
    try
    {
    testRuntimeError();
    }
    catch(ex)
    {
        write(TrimStackTracePath(ex.stack));
    }
})();
write("Passed");

write("Test case 41");
(function test41()
{
    var obj0 = {};
    var arrObj0 = {};
    var func1 = function () {}
    obj0.method0 = func1;
    var IntArr1 = new Array();
    Object.prototype.prop0 = 1;
    var __loopvar0 = 0;
    for (var _strvar20 in arrObj0) {
        if (_strvar20.indexOf('method') != -1)
            continue;
        if (__loopvar0++ > 3)
            break;
        arrObj0[_strvar20] = Math.pow((IntArr1.push(obj0.method0(), (arrObj0.prop1 != arrObj0.prop0), (typeof(obj0.prop0) == 'number'), (typeof(781458996) != 'number'), IntArr1[(((Object.prototype.prop0 >= 0 ? Object.prototype.prop0 : 0)) & 0XF)], (typeof(this.prop0) == 'string'), (typeof(this.prop0) == 'string'))), 1);
        function func22() {
            Math.pow((IntArr1.push(obj0.method0(), (arrObj0.prop1 != arrObj0.prop0), (typeof(obj0.prop0) == 'number'), (typeof(781458996) != 'number'), IntArr1[(((Object.prototype.prop0 >= 0 ? Object.prototype.prop0 : 0)) & 0XF)], (typeof(this.prop0) == 'string'), (typeof(this.prop0) == 'string'))), 1);
        }
    }
})();
write("Passed");

write("Test case 42");
(function test42()
{
  var ary = new Array(10);
  arrObj0 = Object.prototype;
  arrObj0[5] = "temp";
  ary[1] * ((ary.unshift()) - ary[1]);
})();
write("Passed");

write("Test case 43");
(function test43()
{
    //Bug seen with -prejit, adding a generic test case
    var obj0 = {};
    var arrObj0 = {};
    var VarArr0 = [arrObj0];
    var v58 = {
            init: function () {
                return function bar() {
                    arrObj0.prop0;
                };
            }
        };
    CollectGarbage();
    obj0.method1 = v58.init();
    obj0.method1.prototype = {};
    //Property guard should get invalidated
    arrObj0 = new obj0.method1();
})();
write("Passed");

write("Test case 44");
(function test44()
{

  function test0(){
      // Snippet : Native array profile information update

      function v2496(v2497) {
        var v2498 = new Array(v2497);
        for(var v2500 = 0; v2500 < v2497; v2500++) {
            v2498 = v2500;
        }
        return v2498;
      }

      function v2502(v2499) {
        var v2503 = 0;
        for(var v2501 in v2499) {
            v2503 += v2499[v2501];
        }
        return v2503;
      }

      var v2504 = v2496(5);
      v2502(v2504);

      // create missing value and transform the array
      if((1 % 5) <= 3) {
        v2504[v2504.length + 5] = 1;
      }

      var v2505 = v2496(10);
      v2502(v2505);

 };
  // generate profile
  test0();
  test0();
  test0();
})();
write("Passed");

write("Test case 45");
(function test45()
{
    write((Function("return"))());
    write((Function("return;"))());
    write((Function("return 25;"))());

})();
write("Passed");

write("Test case 46");
(function test46()
{
    function compare(a, b){}
    var boundFunction = compare.bind();
    write(Object.getOwnPropertyNames(boundFunction));
})();
write("Passed");

write("Test case 47");
(function test47()
{
    Function('label\n:foo')
    localLabel
     : write("\\n in label accepted");

})();
write("Passed");

write("Test case 48");
(function test48()
{
    var a = Math.random();
})();
write("Passed");
