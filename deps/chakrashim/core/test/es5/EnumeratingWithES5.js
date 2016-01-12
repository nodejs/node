//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var propName = "d";
var propValue = "dvalue";

function CreateSimpleTypeHandlerObject()
{
  var obj = Object.create(null);
  obj[propName] = propValue;
  return obj;
}

function CreateSimpleDictionaryTypeHandlerObject()
{
  var obj = {};
  obj[propName] = propValue;
  return obj;
}

function CreateDictionaryTypeHandlerObject()
{
  var obj = {};
  Object.defineProperty(obj, propName,
    {
      get : function() {},
      configurable : true,
      enumerable : true
    });

  delete obj[propName];
  obj[propName] = propValue;
  return obj;
}

function TestNonWritable(o)
{
  var beforeTestValue = null;
  var value = 1;
  value = TestEnumerations(o, beforeTestValue, value);

  SetWritable(o, propName, false);
  value = TestEnumerations(o, beforeTestValue, value);

  SetWritable(o, propName, true);
  value = TestEnumerations(o, beforeTestValue, value);

  WScript.Echo("Changing writability during enumeration...");
  beforeTestValue = function(o, i, value)
  {
    SetWritable(o, propName, false);
    return value;
  };
  value = TestEnumerations(o, beforeTestValue, value);

  beforeTestValue = function(o, i, value)
  {
    SetWritable(o, propName, true);
    return value;
  };
  value = TestEnumerations(o, beforeTestValue, value);

  WScript.Echo("Freezing object");
  Object.freeze(o);
  beforeTestValue = null;
  value = TestEnumerations(o, beforeTestValue, value);
}

function TestAccessors()
{
  var o = { a:"aValue" };
  DefineAccessor(o, 'b',
    function() { return "GETTER FOR b"; },
    function(v) { WScript.Echo("SETTER FOR b"); }
  );
  o.c = "cValue";  // to be deleted
  o.d = "dValue";

  // Throw in a non-enumerable property
  Object.defineProperty(o, 'e',
    {
      value : "eValue",
      configurable : true,
      writable : true,
      enumerable : false
    });
  DefineAccessor(o, 'f',
    function() { return "GETTER FOR f"; },
    function(v) { WScript.Echo("SETTER FOR f"); }
  );
  o.g = "gValue";

  delete o.c;

  var value = 1;
  var beforeTestValue = null;
  value = TestEnumerations(o, beforeTestValue, value);

  DefineAccessor(o, propName);  
  value = TestEnumerations(o, beforeTestValue, value);

  DefineDataProperty(o, propName, value++);
  value = TestEnumerations(o, beforeTestValue, value);

  WScript.Echo("Defining accessor property during enumeration...");
  beforeTestValue = function(o, i, value)
  {
    if (i === propName) DefineAccessor(o, propName);
    return value;
  };

  value = TestEnumerations(o, beforeTestValue, value);

  WScript.Echo("Defining data property during enumeration...");
  beforeTestValue = function(o, i, value)
  {
    if (i === propName) DefineDataProperty(o, propName, value);
    return value + 1;
  };
  value = TestEnumerations(o, beforeTestValue, value);
}

function SetWritable(o, p, v)
{
  WScript.Echo("Setting writability of " + p + " to " + v);
  Object.defineProperty(o, p, { writable : v });
}

function DefineAccessor(o, p, getter, setter)
{
  if (!getter) getter = function() { return "GETTER"; };
  if (!setter) setter = function(v) { WScript.Echo("SETTER"); }
  WScript.Echo("Defining accessors for " + p);
  Object.defineProperty(o, p,
    {
      get : getter,
      set : setter,
      configurable : true,
      enumerable : true
    });
}

function DefineDataProperty(o, p, v)
{
  WScript.Echo("Defining data property " + p + " with value " + v);
  Object.defineProperty(o, p,
    {
      value : v,
      writable : true,
      configurable : true,
      enumerable : true
    });  
}

function TestEnumerations(o, beforeTestValue, value)
{
  WScript.Echo("Testing for-in enumeration");
  for (var i in o)
  {
    if (beforeTestValue) value = beforeTestValue(o, i, value);
    TestValue(o, i, value++);
  }

  WScript.Echo("Testing getOwnPropertyNames enumeration");
  var names = Object.getOwnPropertyNames(o);
  for (var i = 0; i < names.length; i++)
  {
    if (beforeTestValue) value = beforeTestValue(o, i, value);
    TestValue(o, names[i], value++);
  }

  return value;
}

function TestValue(o, i, value)
{
  WScript.Echo(i + ": " + o[i]);
  WScript.Echo("Setting value to " + value);
  o[i] = value;
  WScript.Echo(i + ": " + o[i]);
}

WScript.Echo("Test 1: Non-writable, simple type handler");
TestNonWritable(CreateSimpleTypeHandlerObject());

WScript.Echo("");
WScript.Echo("Test 2: Non-writable, simple dictionary type handler");
TestNonWritable(CreateSimpleDictionaryTypeHandlerObject());

WScript.Echo("");
WScript.Echo("Test 3: Non-writable, dictionary type handler");
TestNonWritable(CreateDictionaryTypeHandlerObject());

WScript.Echo("");
WScript.Echo("Test 4: Accessors");
TestAccessors();
