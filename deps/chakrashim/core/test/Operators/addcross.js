//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

//
// This test includes Date.toString output. Don't use a baseline.
//
var echo = WScript.Echo;

WScript.LoadScriptFile("adddata.js");
var x = WScript.LoadScriptFile("adddata.js", "samethread");

function addcross(a1, a2) {
    for (var i=0; i<a1.length; ++i) {
        for (var j=0; j<a2.length; ++j) {
            echo("a["+i+"](" + a1[i] +") + a["+j+"]("+a2[j]+") = " + (a1[i] + a2[j]));
        }
    }
}

echo("==== self var + crosscontext var ====");
addcross(this.all, x.all);

echo();

echo("==== crosscontext var + self var ====");
addcross(x.all, this.all);
