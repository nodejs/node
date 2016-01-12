//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// -maxInterpretCount:1 -maxSimpleJitRunCount:1 -dump:lowerer:4 -debug -debugbreak:4 -trace:bailout spread.js
if (this.WScript && this.WScript.LoadScriptFile) { // Check for running in
                                                    // jc/jshost
    this.WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");
}

function transform(a,b,c,d) {
  return a + b + c + d;
}

function fooApply()
{
    return this.transform.apply(this, arguments);
}
fooApply.prototype.transform = transform;

function fooSpread(arglist) {
  return transform(...arglist);
}

function inlinetest() {
  // No bailouts expected
  spreadResult = fooSpread(["1","2","a","4"]);
  applyResult = fooApply("1","2","a","4");
  assert.areEqual(applyResult, spreadResult, "Matching number of args");

  spreadResult = fooSpread([1,"a"]);
  applyResult = fooApply(1,"a");
  assert.areEqual(applyResult, spreadResult, "Not all args");
  
  spreadResult = fooSpread(["xyzzy"]);
  applyResult = fooApply("xyzzy");
  assert.areEqual(applyResult, spreadResult, "One arg (non-X86 corner case)");

  spreadResult = fooSpread([]);
  applyResult = fooApply();
  assert.areEqual(applyResult, spreadResult, "No args");
  
  spreadResult = fooSpread(["1","2","a","4","1","2","a","4","1","2","a","4","1","2","a","4"]);
  applyResult = fooApply("1","2","a","4","1","2","a","4","1","2","a","4","1","2","a","4");
  assert.areEqual(applyResult, spreadResult, "Max number of args (16)");
  
  // Bailouts expected
  spreadResult = fooSpread(["1","2","a","4","1","2","a","4","1","2","a","4","1","2","a","4","bailout"]);
  applyResult = fooApply("1","2","a","4","1","2","a","4","1","2","a","4","1","2","a","4", "bailout");
  assert.areEqual(applyResult, spreadResult, "Max + 1 number of args bailout recovery");
  
  spreadResult = fooSpread([1,2,3,4,5]);
  applyResult = fooApply(1,2,3,4,5);
  assert.areEqual(applyResult, spreadResult, "Native int array bailout recovery");
  
  spreadResult = fooSpread([1.1,2.2,3.3,2.2,1.9]);
  applyResult = fooApply(1.1,2.2,3.3,2.2,1.9);
  assert.areEqual(applyResult, spreadResult, "Native float array bailout recovery");
  
  var missingValues = ["a","b","c",,"d","e"];
  spreadResult = fooSpread(["a","b","c",,"d","e"]);
  applyResult = fooApply(...missingValues);
  assert.areEqual(applyResult, spreadResult, "Missing values array bailout recovery");
}

inlinetest();
inlinetest(); // Force inlinee


// BLUE 592700
function blue592700() {
  (function() { /*sStart*/ ;
      while ((new Array([1, , ])) && 0) {
          yrfgkc(...[1]);
          var yrfgkc = (u3056) => [z1];
      };; /*sEnd*/
  })();
}
blue592700();
blue592700();

// BLUE 592004
function blue592004() {
  function foo() {};
  for (var loopVar2 = 0; loopVar2 < 3; loopVar2++) {
    (function() {
        if (foo) {
            if (foo("w", encodeURIComponent));
        } else lzfqei(...y);
    })();
  }
}
blue592004();
blue592004();

// BLUE 592718
function blue592718() {
  Object.prototype['createFunction'] = function() {};
  var addPropertyName = function() {};
  var y = function() {};
  (function() {
      for (let dkewui = 0; dkewui < 1; ++dkewui) {
          if (dkewui % 10 == 3) { /*hhh*/
              qgjctn(...testForm);
          } else {
              for (var p in y) {
                  addPropertyName(p);
              }
          }
      };; /*sEnd*/
  })();
}
blue592718();
blue592718();

// BLUE 592729
function blue592729() {
  ((encodeURIComponent).bind(..."u")());
}
blue592729();
blue592729();

// BLUE 602272
function test0() {
    var obj1 = {};
    var func1 = function(argStr4) {}
    var func2 = function(argMath8) {
        func1(...[1, arguments[((ui8[1033265332]) >= 0 ? (obj0.length, ui8[1033265332]) : 0)],]);
    }
    obj1.method0 = func2;
    var ui8 = new Uint8Array(256);
    obj1.method0(...[1, 1], 1);
};
test0();
test0();
test0();
test0();
test0();
test0();
test0();
test0();
test0();
test0();
test0();
test0();

WScript.Echo("PASS");