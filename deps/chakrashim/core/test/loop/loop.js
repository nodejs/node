//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


function Ctor()
{
}

Ctor.prototype.blah = function()
{
    for (var i = 0; i < 10; i++)
    {
        eval("this.blahnum= 1000; ");
    }
}


var a = new Ctor();
a.blah();

WScript.Echo(a.blahnum);

var o = {
        f: function(a,b,c) {
                WScript.Echo(a,b,c);
        },

        g: function() {
                for(var i = 0; i < 1; ++i)
                        this.f.apply(this, arguments);
        }
};

o.g(1,2,3);

function f() {
    for (var i = 0; i < 1; i++) {
        var g_args = g.arguments;
        WScript.Echo(g_args === f.caller.arguments);
        WScript.Echo(g_args.callee === f.caller);
    }
}

function g() {
    for (var i = 0; i < 1; i++) {
        f();
    }
}

g('hi');

function test()
{
    with({x:"x"})
    {
        while((function(){ return x; })())
        {
            WScript.Echo(x);
            break;
        }
    }
}
test();

function retval()
{
    for (var i = 0; i < 1; i++)
    {
        if (i > 1)
        {
            return i;
        }
    }
    return 0;
}
retval();

function testForInWithPrototype()
{
    function LoopResultsOfObject(o, methodName)
    {
        WScript.Echo(methodName);
        var method = eval("Object." + methodName);
        var r = method(o);
        WScript.Echo("Length: " + r.length);
        for (var i in r) {
           WScript.Echo(i + " => " + r[i]);
        }
    }

    var protoObject = {}
    protoObject.prop1 = "p1";

    function MyClass()
    {
        this.prop2 = "p2";
    }

    MyClass.prototype = protoObject;

    var instance = new MyClass;

    LoopResultsOfObject(instance, "getOwnPropertyNames");
    LoopResultsOfObject(instance, "keys");
}

testForInWithPrototype();

// Test loop that shouldn't execute and has a side-effect in the loop body
for (var z = 0; z < 0; ++z) {
    1 in 2; 
} 

try {
    eval('for (var a, b in z) {}');
}
catch(e) {
    WScript.Echo(e.message);
}

try {
    Function("for (var a, b in z) {}")();
}
catch(e) {
    WScript.Echo(e.message);
}

try {
    eval('for (a, b in z) {}');
}
catch(e) {
    WScript.Echo(e.message);
}

try {
    Function("for (a, b in z) {}")();
}
catch(e) {
    WScript.Echo(e.message);
}

// Test loop that has bailout in the loop header and must have vars initialized
// (or bailout may try to box garbage values).
function test_bail(){
  var obj0 = {};
  var obj1 = {};
  var func0 = function(){
  }
  var func1 = function(p0,p1,p2){
    var __loopvar2 = 0;
    for(; __loopvar2 < 3 && p2 < (1); __loopvar2++, 14) {
      var __loopvar3 = 0;
      do {
        __loopvar3++;
        obj0.prop0 <<=(ary[(8)] - ((obj2.prop3 ^= (++ obj1.prop1)) ? 1701746938.1 : this.prop2));
        var obj4 = 1;
      } while((1) && __loopvar3 < 3)
    }
    var obj4 = func0(2147483647, (new RegExp("xyz")), 1.1, 1, (7 ? -970342005 : 1));
  }
  var func2 = function(){
    if(ui8[1073741824]) {
      -2;
      var __loopvar3 = 0;
      for(var strvar0 in obj0 ) {
        if(__loopvar3++ > 3) break;
var fPolyProp = function (o) {
    if (o!==undefined) {
        WScript.Echo(o.prop0 + ' ' + o.prop1 + ' ' + o.prop2);
    }
}
fPolyProp(litObj0); fPolyProp(litObj1); fPolyProp(litObj2); 
        obj2.prop5 = 2147483647;
      }
    }
    for(var __loopvar2 = 0; __loopvar2 < 3; __loopvar2++) {
    }
  }
  obj0.method0 = func2;
  var ui8 = new Uint8Array(256);
  var __loopvar1 = 0;
  do {
    __loopvar1++;
    (function(p0, p1, p2, p3){
      var obj5 = obj0;
      obj4 = (new obj5.method0());
    })();
  } while((1) && __loopvar1 < 3)
  if(func1(1, 1)) {
  }
};

// generate profile
test_bail();

// run JITted code
test_bail();


WScript.Echo('done')
