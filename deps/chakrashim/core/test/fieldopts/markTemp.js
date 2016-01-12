//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

//-maxinterpretcount:1
function test0(){
  var obj1 = {};
  var strvar2 = 'nj'+'ax'+'io' + 'sj';
  var strvar4 = 'yi'+'fc'+'ne' + 'wh';
  with (obj1) {
    var strvar2 = strvar4;
  }
  strvar2 +=1;
  WScript.Echo("strvar2 = " + (strvar2));
  WScript.Echo("strvar4 = " + (strvar4));
};
test0();

//-forcenative
var str = "Pas" + "sed";
function foo() {
  var x = "a" + "b";
  var y = str;
  x = y;
  var w = x + " NOT";
  use(w);
}
function use(x) {}
foo();
WScript.Echo(str);
