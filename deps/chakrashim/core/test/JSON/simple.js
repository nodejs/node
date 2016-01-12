//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var jsonBlobL1AsString = "{\"a\":1, \"b\":2}"
var jsonBlobL2AsString = "{\"a\":{\"aa\":10, \"ab\":11}, \"b\":{\"ba\":\"this is\\t a test!\", \"bb\":\"a\"}}"

TraverseAndPrint("level 1:", jsonBlobL1AsString, false);
TraverseAndPrint("level 1+:", jsonBlobL1AsString, true);


TraverseAndPrint("level 1:", jsonBlobL2AsString, false);
TraverseAndPrint("level 1+:", jsonBlobL2AsString, true);


function TraverseAndPrint(msg, str, doRecurse) {
  WScript.Echo("---------------");
  WScript.Echo(msg);
  WScript.Echo(str);

  var json = JSON.parse(str);
  str = "foo";
  WScript.Echo("JSON.Parse result - " + json);

  TraverseJSONObject(msg, 1, json, doRecurse);
  WScript.Echo("---------------");
}

function TraverseJSONObject(msg, level, o, doRecurse) {
  doRecurse = doRecurse || false;
  var sp = "";
  for(var i=1; i<level; i++) {
    sp += "  ";
  }

  for(var l in o) {
    WScript.Echo(msg + sp + l + ": " + o[l]);
    if (doRecurse) {
      TraverseJSONObject(msg, level+1, o[l]);
    }
  }
}
