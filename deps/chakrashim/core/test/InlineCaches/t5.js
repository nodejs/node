//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

/*

"splice" cache transitions among proto and local cache.

*/

var arr = new Array(1);
var newarr = arr.splice(1, 2);

var obj = { };
obj.length = 2;
obj.splice = Array.prototype.splice;
Object.prototype.splice = Array.prototype.splice;

WScript.Echo("ok");