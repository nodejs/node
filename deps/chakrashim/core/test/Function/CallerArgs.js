//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function print(x) { WScript.Echo(x+''); }

function inner(func) {
    print(func.caller);
    if (func.arguments)
    {
        print(func.arguments[0]);
        print(func.arguments.caller);
    }
    if (func.caller) {
        if (func.arguments.caller) {
            print(func.arguments.caller[0]);
        } else {
            print("func.arguments.caller undefined");
        }
    }
    print("");
}


function f() {
    inner(f);
    try {
        try {
            throw null;
        }
        finally {
            inner(g);
        }
    }
    catch (e) {
        inner(f);
    }
}

function g() {
    f("f from g");
}

f("f from global");
g("g from global");

function callerA() {
        AA(null);
}
function AA(x) {
        print(AA.caller);
}

function callerB() {
        eval("AB(null)");
}
function AB(x) {
        print(AB.caller);
}

callerA();
callerB();

(function() {
    print(arguments.caller);
    print(delete arguments.caller);
    print(arguments.caller);
    arguments.caller = 0;
    print(arguments.caller);
    function f() {
        print(arguments.caller);
        print(delete arguments.caller);
        print(arguments.caller);
        arguments.caller = 0;
        print(arguments.caller);
    }
    f();
})();

function test0(){
  var func0 = function(){
    var __loopvar1 = 0;
    while(((b <<= (arguments.caller && arguments.caller[1]) ? 3 : 1)) && __loopvar1 < 3) {
      __loopvar1++;
    }
  }
  var func2 = function(){
    func0(); 
  }
  var b = 1;
  function bar0 () {
      func2();
  }
  bar0(1, 1, 1); 
  WScript.Echo("b = " + (b|0));
};

// generate profile
test0(); 
test0(); 
