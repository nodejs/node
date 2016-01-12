//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(args)
{
 WScript.Echo(args);
}

write("TestCase1");
write(Object.seal.length);
write(Object.isSealed({}));

write("TestCase2 - seal & add a property");
var a = {x:20, y:30};
Object.seal(a);
SafeCall(function() { a.x = 40; });
SafeCall(function() { a.z = 50; });
write(Object.getOwnPropertyNames(a));
write(Object.isSealed(a));
write(a.x);

write("TestCase3 - seal & delete a property");
var a = {x:20, y:30};
Object.seal(a);
SafeCall(function() { a.x = 40; });
SafeCall(function() { delete a.x; });
SafeCall(function() { a.z = 50; });
write(Object.getOwnPropertyNames(a));
write(Object.isSealed(a));
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