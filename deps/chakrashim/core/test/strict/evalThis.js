//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

"use strict";

var echo = WScript.Echo;

echo("** Checking 'this' using 'eval' in global scope");
eval("echo(this);");
eval("'use strict'; echo(this);");
eval("eval('echo(this);');");
eval("'use strict'; eval('echo(this);');");

echo("** Checking 'this' using 'my_eval' in global scope");
var my_eval = eval;
my_eval("echo(this);");
my_eval("'use strict'; echo(this);");
my_eval("eval('echo(this);');");
my_eval("'use strict'; eval('echo(this);');");

function foo() {
    echo("** Checking 'this' using 'eval' in function scope");
    eval("echo(this);");
    eval("'use strict'; echo(this);");
    eval("eval('echo(this);');");
    eval("'use strict'; eval('echo(this);');");

    echo("** Checking 'this' using 'my_eval' in function scope");
    var my_eval = eval;
    my_eval("echo(this);");
    my_eval("'use strict'; echo(this);");
    my_eval("eval('echo(this);');");
    my_eval("'use strict'; eval('echo(this);');");
}
foo();
