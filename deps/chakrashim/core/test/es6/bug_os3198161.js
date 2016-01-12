//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

obj = [1,2,3,4,5];
obj.constructor = function() { return {}; }
Object.freeze(obj);
if ('[2,3]' == JSON.stringify(Array.prototype.slice.call(obj,1,3)))
{
    WScript.Echo('Pass');
}
else
{
    WScript.Echo('Fail');
}