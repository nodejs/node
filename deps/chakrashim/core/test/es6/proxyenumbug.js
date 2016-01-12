//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var obj = {foo:1, bar: 2};
var iterator = Reflect.enumerate(obj);
var passed = 1;
if (typeof Symbol === 'function' && 'iterator' in Symbol) {
  passed &= Symbol.iterator in iterator;
}
var item = iterator.next();
passed &= item.value === 'foo' && item.done === false;
item = iterator.next();
passed &= item.value === 'bar' && item.done === false;
item = iterator.next();
passed &= item.value === undefined && item.done === true;
if (passed) {
WScript.Echo("PASS");
}
