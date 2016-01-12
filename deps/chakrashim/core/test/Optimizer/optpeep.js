//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var Count = 0;
var shouldBailout = false;
function test0(){
  var obj0 = {};
  var func0 = function(p0){
    if((shouldBailout ? (Object.defineProperty(obj0, 'prop6', {get: function() { Count++; return 3; }, set: function(_x) { WScript.Echo('obj0.prop6 setter'); }}), 1) : 1)) {
      ((Math.log(obj0.prop6) - Boolean));
11    }
  }
  obj0.prop1 = {prop0: 1, prop1: 1, prop2: 1, prop3: 1, prop4: 1, prop5: 1, prop6: func0(1, 1), prop7: 1};
};

for (var i = 0; i < 2000; i++)
{
    test0();
}


// run code with bailouts enabled
shouldBailout = true;
test0();

if (Count != 1)
{
    WScript.Echo("FAILED\n");
}
else
{
    WScript.Echo("Passed\n");
}

// Windows Blue Bug 416975
function test1(){
  function bar4 (){
    if(bar4())
    {
     }
    return (1 > 1);
  }
  bar4();
};

try{
    test1(); 
}
catch (e) {}
