//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// uses es6 string template as validation for ES6 'stable' features

function echo(str) {
    WScript.Echo(str);
}

var world="WoRlD!";
echo(`hello${world}`);

