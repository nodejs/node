//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var maxPushArgs = 3;  // Maximum number of arguments to push supported by this test case.  Cannot be greater than 6.

for (var b = 0; b <= 1; b++) {
    var isArray = (b === 0);
    for (var i = 1; i <= maxPushArgs; i++) {
        var testType = isArray ? "Array" : "Object";
        WScript.Echo(testType + " Test " + i);
        var a = Create(isArray);
        WScript.Echo("Pushing...");
        PushArgs(a, i, "Bef");
        Output(a, i);
        WScript.Echo("Pushing...");
        PushArgs(a, 1, "Aft");
        Output(a, i + 1);
    }
}

function Create(isArray)
{
  var arr;
  if (isArray)
  {
    arr = [];
  }
  else
  {
    arr = {};
    arr.push = Array.prototype.push;
    arr.length = 4294967294;
  }

  arr[0]="Value0";
  arr[1]="Value1";
  arr[2]="Value2";
  arr[4294967293] = "Value4294967293";

  return arr;
}

function PushArgs(arr, num, prefix) {
    if (num < 1 || num > maxPushArgs) {
        WScript.Echo("FAIL");
        return;
    }
    try {
        if (num === 1) {
            arr.push(prefix + "1");
        }
        else if (num === 2) {
            arr.push(prefix + "1", prefix + "2");
        }
        else if (num === 3) {
            arr.push(prefix + "1", prefix + "2", prefix + "3");
        }
        WScript.Echo("No exception");
    }
    catch (e) {
        WScript.Echo(e.name + ": " + e.message);
    }
}

function Output(arr, numPushed)
{
  if (numPushed > maxPushArgs) {
      WScript.Echo("FAIL");
      return;
  }
  WScript.Echo("Length is: " + arr.length);
  OutputIndex(arr, 0);
  OutputIndex(arr, 1);
  OutputIndex(arr, 2);
  for (var i = 0; i <= numPushed; i++)
  {
    var index = "429496729" + (3 + i); // Does not work if maxPushArgs > 6
    OutputIndex(arr, index);
  }
}

function OutputIndex(arr, index)
{
  var v = arr[index];
  if (v == undefined)
  {
    v = "UNDEFINED";
  }
  WScript.Echo(index + ": " + v);
}

//implicit calls
function foo()
{
    var obj = {};
    Object.prototype.push = Array.prototype.push;
    var x;
    Object.defineProperty(obj, "length", {get: function() {x = true; return 5;}});
    x = false;

    try
    {
        var len = obj.push(1);
    }
    catch (e)
    {
        WScript.Echo('caught exception calling push');
    }

    WScript.Echo(x);
    return len;
}

WScript.Echo (foo());

function bar()
{
    var a = Number();
    Number.prototype.push = Array.prototype.push;
    a.push(1);
}
bar();

function test0(arr)
{
    for(var __loopvar4 = 0; __loopvar4 < 2; __loopvar4++)
    {
      arr.length --;
      arr.push(3);
    }
    return arr.length;
}

WScript.Echo("ary.length = " + test0(new Array(10)));

function popTest() {
    [ , ].pop();
};
WScript.Echo(popTest());
