//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var echo = WScript.Echo;

function top1() {
    "use strict";
    function nested1() {
        echo(this);
    }
    echo(this);
    eval("nested1();");
}

function top2() {
    function nested2() {
        "use strict";
        echo(this);
    }
    echo(this);
    eval("nested2();");
}

function top3() {
    function nested3() {
        echo(this);
    }
    echo(this);
    eval("'use strict'; nested3();");
}

top1();
top2();
top3();
