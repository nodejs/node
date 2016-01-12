//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Constructor providers
var nonNumeric = function()
{
  return function()
  {
    this.a = 1;
    this.b = 2;
    this.c = 3;
  };
}
nonNumeric.myName = "Non-numeric";
nonNumeric.prop = 'b';

var numeric = function()
{
  return function()
  {
    this.a = 1;
    this[9] = 2;
    this.c = 3;
  };
}
numeric.myName = "Numeric";
numeric.prop = '9';

var ctorProviders = [ nonNumeric, numeric ];

// Property definers
var doNothing = function(obj, prop) {}
doNothing.myName = "No-op";

var defineAccessor = function(obj, prop)
{
  Object.defineProperty(obj, prop,
    {
      set:function(v) { WScript.Echo("SETTER: " + v); },
      get:function() { WScript.Echo("GETTER"); return "GETTERVALUE";}
  });
}
defineAccessor.myName = "Define accessor property";

var defineNonWritable = function(obj, prop)
{
  Object.defineProperty(obj, prop, { value:"NONWRITABLE", writable:false } );
}
defineNonWritable.myName = "Define non-writable property";

var defineWritable = function(obj, prop)
{
  Object.defineProperty(obj, prop, { value:"WRITABLE", writable:true } );
}
defineWritable.myName = "Define writable property";

var definers = [doNothing, defineAccessor, defineNonWritable, defineWritable];

var testId = 1;

for (var i = 0; i < definers.length; i++)
{
  var define = definers[i];
  for (var j = 0; j < ctorProviders.length; j++)
  {
    var provider = ctorProviders[j];
    var ctor = provider();
    WScript.Echo("Test " + testId++ + ": " + provider.myName + ", " + define.myName);
    var o = new ctor();
    print(o, provider.prop);
    define(ctor.prototype, provider.prop);
    o = new ctor();
    print(o, provider.prop);
  }
}

function print(o, prop)
{
  WScript.Echo("a: " + o.a);
  WScript.Echo(prop + ": " + o[prop]);
  WScript.Echo("c: " + o.c);
}
