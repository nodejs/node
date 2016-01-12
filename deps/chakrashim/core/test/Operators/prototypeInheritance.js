//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var actual = [];

function UpdateActual(v)
{
  Update(actual, v);
}

function Update(arr, value)
{
  arr[arr.length] = value;
}

function CreateObjects()
{
  return {
     objectNoProps  : {},
     objectOneProp  : { a : 1 },
     objectSDTH     : { a1: 1, a2:2, a3:3, a4:4, a5:5, a6:6, a7:7, a8:8, a9:9, a10:10, a11:11, a12:12, a13:13, a14:14, a15:15, a16:16, a17:17 }, // SimpleDictionaryTypeHandler
     objectWithArr  : { a : 1, 0 : 0},
     objectCreate   : Object.create(null),
    "arr          " : [],
    "func         " : function(x, y) {},
    "regex        " : /abc/g,
    "date         " : new Date()
  }
}

var propertyProviders = {
  "doNothing  " : function(o) { },
  "addOneProp " : function(o) { o.b = 0 },
  "addTwoProps" : function(o) { o.b = 0; o.c = 0; },
  "addIndex   " : function(o) { o[2] = 0; }
};

var propertiesToTest = {
  "numeric   " : 2,
  "nonNumeric" : "foo"
};

function defineAccessors(obj, prop)
{
  Object.defineProperty(obj, prop, {
    get: function() { UpdateActual("get"); return "getValue";},
    set: function(v) { UpdateActual("set");  }
  });
}

function defineNonWritable(obj, prop)
{
  Object.defineProperty(obj, prop, {
    writable : false,
    value : "protoValue"
  });
}

function defineAndFreeze(obj, prop)
{
  obj[prop] = "protoValue";
  Object.freeze(obj);
}

function defineAndSeal(obj, prop)
{
  obj[prop] = "origProtoValue";
  Object.seal(obj);
  Object.defineProperty(obj, prop, {
    writable : false,
    value : "protoValue"
  });
}

var definers = {
  "setter     " : defineAccessors,
  "nonWritable" : defineNonWritable,
  "freezer    " : defineAndFreeze,
  "sealer     " : defineAndSeal
};

var preparers = {
  "newProp     " : function(o, prop) {},
  "existingProp" : function(o, prop) { o[prop] = "toBeOverwritten"; }
}

var testNum = 1;

for (var definerKey in definers)
{
  for (var providerKey in propertyProviders)
  {
    for (var propKey in propertiesToTest)
    {
      for (var i = 0; i <= 1; i++)
      {
        for (var prepKey in preparers)
        {
          var objects = CreateObjects();
          for (var objKey in objects)
          {
            var shadow = (i === 0);
            
            var description = "Test " + testNum++ + ": " + definerKey + ", " + objKey + ", " +  providerKey + ", " + propKey + ", " + prepKey +
              ", shadow=" + shadow;

            WScript.Echo(description);

            actual = [];

            var define = definers[definerKey];
            var proto = objects[objKey];
            var prop = propertiesToTest[propKey];
            var provider = propertyProviders[providerKey];
            var prepare = preparers[prepKey];

            provider(proto);
            var subProto = Object.create(proto);
            var base = Object.create(subProto);

            if (shadow)
            {
              subProto[prop] = "shadowValue";
            }

            prepare(proto, prop);
            define(proto, prop);        

            base[prop] = "baseValue";
            var v = base[prop];
            UpdateActual(v);

            var isSetter = (definerKey.indexOf("setter") >= 0);

            var expected = [];
            if (isSetter && !shadow)
            {
               Update(expected, "set");
               Update(expected, "get");
            }

            if (shadow)
            {
              Update(expected, "baseValue");
            }
            else if (isSetter)
            {
              Update(expected, "getValue");
            }
            else
            {
              Update(expected, "protoValue");
            }

            var failed = false;

            if (actual.length != expected.length)
            {
              failed = true;
            }
            else
            {
              for (var k = 0; k < actual.length; k++)
              {
                if (actual[k] !== expected[k])
                {
                  failed = true;
                  break;
                }
              }
            }

            if (failed)
            {
              WScript.Echo("FAILED: " + description);
              WScript.Echo("Expected: " + ArrayToString(expected));
              WScript.Echo("Actual: " + ArrayToString(actual));
            }
            else
            {
              WScript.Echo("PASS");
            }
          }
        }
      }
    } 
  }
}

function ArrayToString(arr)
{
  var str = "";
  while (arr.length > 0)
  {
    str += arr.shift();
  }

  return str;
}