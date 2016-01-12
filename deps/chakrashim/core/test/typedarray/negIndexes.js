//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var arr = new Float64Array(100);

function foo()
{
  for (var k = 0; k > -10; k--) {
    arr[k] = 5.1*k;
  }
}

function bar()
{
    for (var k = 0; k > -10; k--) {
    WScript.Echo(arr[k]);
    }
}

for (var i = 0; i < 1000; i++)
{
    foo();
}
bar();
