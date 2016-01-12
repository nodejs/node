//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


function x()
{
  var e = eval;
}
x();

var bar = function (e) {
    e.apply(this);
}

function foo() {

  WScript.Echo("pass");
}

bar(foo);
