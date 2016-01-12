//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// -nonative -InjectExceptionAtPartiallyInitializedInterpreterFrame:3 -InjectPartiallyInitializedInterpreterFrameErrorType:1

// We only check interpreter frame which ret addr matches one from frames pushed to scriptContext.
// Thus use same function body (causes same interpreter thunk).

function createFoo()
{
  var foo = function(another)
  {
    if (another) another();
  }
  return foo;
}

try
{
  var foo1 = createFoo();
  var foo2 = createFoo();
  foo1(foo2);
}
catch (ex)
{
  var stackAfterTrimDirectoryName = ex.stack.replace(/\(.*\\/g, "(");
  WScript.Echo(stackAfterTrimDirectoryName);
}
