//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// -maxInterpertCount:1 -off:SimpleJit

function f(x) 
{ 
  if (x < 0) {
    throw Error("abc");
  } else {
    f(x-1);
  }
}

try 
{
   f(1);
} catch (ex) 
{
  var stackAfterTrimDirectoryName = ex.stack.replace(/\(.*\\/g, "(");
  WScript.Echo(stackAfterTrimDirectoryName);
}
