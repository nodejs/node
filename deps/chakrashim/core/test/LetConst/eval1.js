//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Let/const redecl/reassign cases in presence of eval.
// Eval creates its own block scope, preventing let and const variables from leaking out.

function write(x) { WScript.Echo(x) }

// Global scope.
const z = 'global z';
let w = 'global w';

eval('let x = "global x"; const y = "global y"; write(z);');

try { write(x); } catch (e) { write(e); }
try { write(y); } catch (e) { write(e); }

// Try redeclaration at global scope.
try {
    eval('var z = "global var z";');
}
catch(e) {
    write(e);
}
try {
    eval('var w = "global var w";');
}
catch(e) {
    write(e);
}

// Block scope in global function.
try {
    const z = 'global block z';

    eval('let x = "global block x"; const y = "global block y"; write(z);');

    try { write(x); } catch (e) { write(e); }
    try { write(y); } catch (e) { write(e); }

    // function declared in global block.
    outer();

    function outer() {
        let w = 'outer w';

        // Try redeclaration at function scope.
        try {
            eval('var w = "outer var w";');
        }
        catch(e) {
            write(e);
        }
        write(w);

        try {
            const z = 'outer z';

            eval('let x = "outer x"; const y = "outer y"; write(z);');

            try { write(x); } catch (e) { write(e); }
            try { write(y); } catch (e) { write(e); }

            // Try assigning const y; shouldn't see const y and instead create function var y
            eval('y = "outer var y";');
            write(y);

            // function nested within function body.
            inner();
            write(y);

            function inner() {
                let w = 'inner w';

                // Try redeclaration at function scope.
                try {
                    eval('var w = "inner var w";');
                }
                catch(e) {
                    write(e);
                }
                write(w);

                try {
                    const z = 'inner z';

                    // const y shouldn't affect outer y
                    eval('let x = "inner x"; const y = "inner y"; write(z);');

                    try { write(x); } catch (e) { write(e); }
                    write(y); // outer var y
                }
                catch(e) {
                    write(e);
                }

                function foo() {
                    let yy = "b";
                    const yx = "a";
                    yy += "a";
                    eval("WScript.Echo(yy);")
                    WScript.Echo(yy);
                }
                foo();
            }
        }
        catch(e) {
            write(e);
        }
    }
}
catch(e) {
    write(e);
}

// BLUE Bug 454963 (shouldn't crash)
{
    with ({})
        eval("");
    function f() { x; }
    let x;
}

this.eval('let x = 0; function f() { return x; }; WScript.Echo(f());');
