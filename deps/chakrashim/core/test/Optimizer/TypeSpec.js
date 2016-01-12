//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Conv_Num gets changed to Nop during copy-prop. The Nop instruction should not go through type specialization.
function test0(a) {
    a &= 1;
    var b = 1 << a;
    var c = 0;
    if(a)
        c = (b = +b) * 1.1;
    return ~b;
}
WScript.Echo("test0: " + test0(1));

// - '~o' is not int-specialized but produces an int-range value, though 'a' is available only as a var
// - 'a === 1.1' is float-specialized, so now 'a' is also available as a float64
// - 'a = 1' makes 'a' only available as an int32 in the branch
// - Now we have a.v and a.f live on one side with 'a' having an int value, and a.i on the other side. The merge needs to keep
//   a.v live on both sides since due to the int value, 'a' may be int-specialized later (in 'a + 1'). a.v needs to be available
//   since a.f cannot be losslessly converted into an int32 (expensive and not supported at the time of this writing).
function test1() {
    var o = {};
    var a = ~o;
    if(a === 1.1)
        a = 1;
    return a + 1;
}
WScript.Echo("test1: " + test1());

var shouldBailout = false;
function testrem(){
  function leaf(x) { return x; };
  var obj0 = {};
  var func0 = function(p0,p1,p2){
    return (g += k);
  }
  var func1 = function(p0){
    // Make sure that we track implicit register kills properly for this type-specialized %.
    k -=((((r ^ (++ q)) % (~ 1)) * (1 - ((shouldBailout ? func0 = leaf : 1), func0(1, 1)))) ? 1 : d);
  }
  var func2 = function(p0,p1,p2){
    func0(1, 1, 1, 1, (-- d), 1);
  }
  obj0.method0 = func2;
  var b = 2147483647;
  var d = -984599814;
  var g = 1;
  var k = 1;
  var q = 7.26957791630936E+18;
  var r = -264469094;
  obj0.prop0 = 1;
  obj0.length = {prop7: 1, prop6: 1, prop5: 1, prop4: 1, prop3: 1, prop2: 1, prop1: (q += b), prop0: (func1() * obj0.method0(1, 1, 1, 1))};
  ((obj0.prop0 -= g));
  WScript.Echo("obj0.prop0 = " + (obj0.prop0|0));
};

testrem();
testrem();

