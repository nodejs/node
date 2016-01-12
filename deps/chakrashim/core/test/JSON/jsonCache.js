//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


// Spaces in JSON strings are needed to ensure json cache kicks in

// An example of showing path type divergence a->b, a->b, a->b, a->c, a->b
function test1()
{
    var jsonString = "                                                                      [ {\"a\":1, \"b\":2}, {\"a\":1, \"b\":2}, {\"a\":1, \"b\":2}, {\"a\":1, \"c\":2}, {\"a\":1, \"b\":2} ]"
    var json = JSON.parse(jsonString);
    TraverseJSONObject("Path type divergence" , 1, json, true);
}

// An example of showing numerical property a->b->1->c, a->b->1->c
function test2()
{
    var jsonString = "                                                                       [ {\"a\":1, \"b\":2, \"1\":3, \"c\":4 }, {\"a\":1, \"b\":2, \"1\":3, \"c\":4 }, {\"a\":1, \"b\":2, \"1\":3, \"c\":4 }, {\"a\":1, \"b\":2, \"1\":3, \"c\":4 }] "
    var json = JSON.parse(jsonString);
    TraverseJSONObject("Numerical properties" , 1, json, true);
}

// Tests having duplicate property names
function test3()
{
    var jsonString = "                                                                       [ {\"a\":1, \"b\":2, \"c\":3, \"d\":4,\"a\":5, \"a\":6, \"b\":7 }, {\"a\":1, \"b\":2, \"c\":3, \"d\":4,\"a\":5, \"a\":6, \"b\":7 }, {\"a\":1, \"b\":2, \"c\":3, \"d\":4,\"a\":5, \"a\":6, \"b\":7 }, {\"a\":1, \"b\":2, \"c\":3, \"d\":4,\"a\":5, \"a\":6, \"b\":7 }] "
    var json = JSON.parse(jsonString);
    TraverseJSONObject("Duplicates" , 1, json, true);
}   

function TraverseJSONObject(msg, level, o, doRecurse) {
  doRecurse = doRecurse || false;
  var sp = "";
  if(level == 1)
  {
    WScript.Echo(msg);
  }
  for(var i=1; i<level; i++) {
    sp += "    ";
  }

  for(var l in o) {
    WScript.Echo(sp + l + ": " + o[l]);
    if (doRecurse) {
      TraverseJSONObject(msg, level+1, o[l]);
    }
  }
}

function RunAll()
{
   WScript.Echo("Running test1...");
   test1();
   WScript.Echo("Running test2...");
   test2();
   WScript.Echo("Running test3...");
   test3();
}
RunAll();

