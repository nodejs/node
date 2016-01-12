//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0() {
    var obj0 = {};
    obj0.prop1 = { prop0: obj0.prop1, prop1: 1 };
    obj0.length = 1;
    WScript.Echo("obj0.prop1 = " + (obj0.prop1 | 0)); ;
    WScript.Echo("obj0.length = " + (obj0.length | 0)); ;
};

// generate profile
test0();

// run JITted code
test0();

function test1() {
    var obj0 = {};
    var obj1 = {};
    var func2 = function (p0, p1) {
        var __loopvar4 = 0;
        do {
            __loopvar4++;
        } while (((Math.exp((--obj1.prop1)) ^ (this.prop0 /= 1))) && __loopvar4 < 3)
    }
    obj1.method0 = func2;
    for (var __loopvar0 = 0; obj0.prop2 < (obj1.method0(1)) && __loopvar0 < 3; obj0.prop2++ + __loopvar0++) {
    }
};

// generate profile
test1();

// run JITted code
test1();

function test2() {
        for (knbnbs = 0; knbnbs < 0; ++knbnbs) {
                (-3 / 0+1);
        }; ;
};
test2();
test2();

// Test interaction with int type spec
function test3() {
    var x = {};
    try {
        x = u3056;
    } catch (y) {};
    u3056 = NaN;
    (function () {
        (x.d %= Math.exp(x));
    })();
};

test3();
test3();

// Test case where the type reference is hoisted, but the type is not live on the back edge.
function test4(){
  var obj1 = {};
  this.prop4 = 2147483647;
  this.prop5 = 1;
  Object.prototype.prop1 = 1;
  Object.prototype.prop5 = 1;
  for (var __loopvar2 = 0; __loopvar2 < 3 && obj1.prop1 < ((-- this.prop4)); __loopvar2++ + obj1.prop1++) {
    var __loopvar3 = 0;
    while ((this.prop5) && __loopvar3 < 3) {
      __loopvar3++;
      obj1.prop0 = this.prop4;
    }
    delete this.prop5;
  }
  WScript.Echo("obj1.prop0 = " + (obj1.prop0|0));
};

// generate profile
test4();

// run JITted code
test4();

// Test case where a ToVar is inserted and its effect must be reflected.
function test5(){
    var obj0 = {};
    obj0.prop0 = 1;
    Math.floor(1);
    var i = 0;
    do {
        obj0.prop0;
        obj0 = 1;
        obj0.prop0;
        i++;
    } while (i < 3)
}
test5();
test5();
