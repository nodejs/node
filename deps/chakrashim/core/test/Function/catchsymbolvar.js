//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function echo(m) { this.WScript ? WScript.Echo(m) : console.log(m); }

function foo() {
    function bar() {
        eval("throw new Error('bar')");
    }

    try {
        bar();
    } catch (e) {
        var e = new Error(); // Win8: 749251. "e" is catch symbol.
        echo("pass");
    }
}

foo();
