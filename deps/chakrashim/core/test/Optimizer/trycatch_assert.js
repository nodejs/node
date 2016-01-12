//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var shouldBailout = false;
function func0(){
 var loopInvariant = shouldBailout ? 12 : 11;
  var obj0 = {};
    for(var __loopvar3 = loopInvariant - 6;;) {
      try {
        try {
          obj0.randomFunc();
        } catch(ex) {
          WScript.Echo(ex.message);
        }

        var __loopvar6 = loopInvariant - 3;
        do {
          __loopvar6++;
          if (__loopvar6 == loopInvariant + 1) break;
          if(shouldBailout){
            return  'somestring'
          }
        } while((1))
      } catch(ex) {
        WScript.Echo(ex.message);
1      }
      if (__loopvar3 == loopInvariant) break;
      __loopvar3 += 2;
    }
  }
function test0(){
  var obj0={};
  obj0.prop0 *=func0.call(obj0);
};

test0();
test0();
test0();
test0();
test0();
shouldBailout = true;
test0();

function test1(){
  var obj0 = {};
  var obj1 = {};
  var FloatArr0 = [-1,2038362539.1,570586731,4.71064707708417E+18,-276000689.9,-142,65535,369612157.1];
  protoObj0 = Object.create(obj0);
  protoObj1 = Object.create(obj1);
  try {
    obj1 = 1;
    var id29 = FloatArr0[(18)];
    var strvar10 = 1;
    strvar10 = strvar10.substring();
    obj1 = {};
    WScript.Echo(obj1);
  } catch(ex) {
    WScript.Echo(ex.message);
    var __loopvar3 = 16;
    while((((protoObj0.length >= obj1.length)||(obj0.prop0 >= protoObj1.prop0)))) {
      if (__loopvar3 == 4) break;
      __loopvar3 -= 4;
    }
  }
};

test1();
test1();
test1();

function test2() {
  var y = function () {
  };
  try {
    try {
      c;
    } catch (x) {
      y = [z1];
    }
  } catch (e) {
  }
  WScript.Echo(y);
}
test2();
test2();
test2();

WScript.Echo("Passed");

var obj00 = {i: 0,
            next: function(){
                if(this.i != 75)
                    return this.i++;
                throw this.i;
            }
            };

var obj11 = {prop0: {x:1}};
function test3(obj)
{
    var a;
    var b = obj11.prop0;
    try {
        while (true) {
            a = obj.next();
        }
    } catch (e) {
        b.x;
    }

}
test3(obj00);
obj00.i = 0;
test3(obj00);
WScript.Echo("Passed");
