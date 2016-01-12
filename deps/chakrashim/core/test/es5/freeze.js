//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(args)
{
 WScript.Echo(args);
}

write("TestCase1");
write(Object.freeze.length);
write(Object.isFrozen({}));

write("TestCase2 - freeze & add a property");
var a = {x:20, y:30};
Object.freeze(a);
SafeCall(function() { a.z = 50; });
write(Object.getOwnPropertyNames(a));
write(Object.isFrozen(a));

write("TestCase3 - freeze & delete a property");
var a = {x:20, y:30};
Object.freeze(a);
SafeCall(function() { delete a.x; });
write(Object.getOwnPropertyNames(a));
write(Object.isFrozen(a));
write(a.x);

write("TestCase4 - freeze & modify a property");
var a = {x:20, y:30};
Object.freeze(a);
SafeCall(function() { a.x = 40; });
SafeCall(function() { a.y = 60; });
write(Object.getOwnPropertyNames(a));
write(Object.isFrozen(a));
write(a.x);

function SafeCall(f)
{
  try
  {
    f();
  }
  catch (e)
  {
    write("Exception: " + e.name);
  }
}