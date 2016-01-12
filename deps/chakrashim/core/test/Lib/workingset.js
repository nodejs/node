//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.Echo('hello');
var a = WScript.GetWorkingSet();
WScript.Echo("workingset = " + a.workingSet);
WScript.Echo("maxworkingset = " + a.maxWorkingSet);
WScript.Echo("pagefaultcount = " + a.pageFault);
WScript.Echo("private usage = " + a.privateUsage);

function print(obj, name)
{
  WScript.Echo("print object " + name);
  for (i in obj)
  {
    WScript.Echo(i + ' = ' + obj[i]);
  }
}

var c = Debug.getHostInfo();
print(c, "hostinfo");

var d = Debug.getMemoryInfo();
for (i in d)
{
print(d[i], i);
}

var b = Debug.getWorkingSet();
WScript.Echo("workingset = " + b.workingSet);
WScript.Echo("maxworkingset = " + b.maxWorkingSet);
WScript.Echo("pagefaultcount = " + b.pageFault);
WScript.Echo("private usage = " + b.privateUsage);
