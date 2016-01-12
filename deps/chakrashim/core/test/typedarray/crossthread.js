//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var fileNames= ["dataview.js", "int8array.js", "uint8array.js", "int16array.js", "uint16array.js",
    "int32array.js", "uint32array.js", "float32array.js", "float64array.js"];

for (var i = 0; i < fileNames.length; i++)
{
   WScript.Echo("testing file " + fileNames[i]);
   oneFile(fileNames[i]);
}

function oneFile(fileName)
{
  var frame = WScript.LoadScriptFile(fileName, "crossthread");
  WScript.Echo("Start two thread test on file " + fileName);
  for (var i in frame)
  {
      WScript.Echo("property of global: " + i);
      if (i == 'WScript')
         continue;
      if (typeof frame[i] == "object")
      {
         for (var j in frame[i])
           WScript.Echo("sub object " + j + " in " + i + " is " + frame[i][j]);
      }
    try 
    {
      if (typeof frame[i] == "function" && frame[i].length == 0)
      {
        frame[i]();
      }
    }
    catch (e)
    {
      WScript.Echo("exception is " + e.number + e.description);
    }
  }
}