//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }
write("Only function code is in strict mode");

var a = 02;
write(a);
try
{
    eval(" function f() { 'use strict'; a = 01; }");
    f();
}
catch(e)
{
    write(e);
}

let yield;
function test0() {
    function test0_1() {
        "use strict";
    }
    yield = 'yield okay';
}
test0();

write(yield);

try {
    eval('function test1() {' +
         '   "use strict";' +
         '    function test1_1() {' +
         '    }' +
         '    yield;' +
         '}');
}
catch(e) {
    write(e);
}

try {
    eval('function outer() {' +
            '(function() {' +
            '"use strict";' +
            'with({}){}' +
            '})();' +
         '}');
}
catch(e) {
    write(e);
}
