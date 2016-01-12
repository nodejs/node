//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var noMessage = new Error();
var withMessage = Error("I have a message for you...");

WScript.Echo("Error.prototype.name: " + Error.prototype.name);
WScript.Echo("Error.prototype.message: " + Error.prototype.message);
WScript.Echo("Error.prototype.toString(): " + Error.prototype.toString());
//WScript.Echo("Error.prototype.constructor: " + Error.prototype.constructor);

WScript.Echo("noMessage.name: " + noMessage.name);
WScript.Echo("noMessage.message: " + noMessage.message);
WScript.Echo("noMessage.toString(): " + noMessage.toString());

WScript.Echo("withMessage.name: " + withMessage.name);
WScript.Echo("withMessage.message: " + withMessage.message);
WScript.Echo("withMessage.toString(): " + withMessage.toString());

Error.prototype.name = "TryNewName";
WScript.Echo("Changing Error.prototype.name to TryNewName...");
WScript.Echo("Error.prototype.name: " + Error.prototype.name);
WScript.Echo("withMessage.name: " + withMessage.name);

try
{
  RangeError.prototype.message = "Range Error's prototype";
  throw RangeError.prototype;
}
catch(ex)
{
   WScript.Echo("Message: " + ex.message);
}