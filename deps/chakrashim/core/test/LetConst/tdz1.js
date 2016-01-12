//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// ES6 block-scoping with closures, switches

var x = 'global x';

function write(x) { WScript.Echo(x); }

(function() {
    try {
        inner();
    }
    catch(e) {
        write(e);
    }

    let x = 'local x';

    try {
        inner();
    }
    catch(e) {
        write(e);
    }

    function inner() {
        write(x);
    }
})();

(function() {
    while(1) {
        try {
            LABEL0:
            switch(x) {
            case 0:
                let x = 'local x';
                // fall through
            default:
                function inner() {
                    write(x);
                }
                inner();
                // Should only get here after executing case 0.
                return;
            case 'seriously?':
                break LABEL0;
            }
        }
        catch(e) {
            write(e);
            // Should assign to global x.
            x = 0;
        }
    }
})();

try {
    (function() {
        if (typeof a === 'string') {
            write('a is a string');
        }
        let a = 'let a';
    })();
} catch (e) {
    write(e);
}

try {
    (function() {
        let a = 'let a';
        if (typeof a === 'string') {
            write('a is a string');
        }
    })();
} catch (e) {
    write(e);
}

try {
    (function () {
        if (delete a) {
            write("deleted a");
        } else {
            write("did not delete a");
        }
        let a = 'let a';
    })();
} catch (e) {
    write(e);
}

try {
    (function () {
        let a = 'let a';
        if (delete a) {
            write("deleted a");
        } else {
            write("did not delete a");
        }
    })();
} catch (e) {
    write(e);
}

try {
    try {
        write(eval('typeof t'));
    }
    catch(e) {
        write(e);
    }

    (function() {
        write(eval('typeof t'));
    })();
}
catch(e) {
    write(e);
}

let t;
write(eval('typeof t'));

(function () {
    try {
        let foo = eval('foo();');
    } catch (e) {
        write(e);
    }

    try {
        const foo = eval('foo();');
    } catch (e) {
        write(e);
    }

    try {
        eval('foo();');
        let foo = 123;
    } catch (e) {
        write(e);
    }

    try {
        eval('foo();');
        const foo = 123;
    } catch (e) {
        write(e);
    }
})();

(function () {
    try {
        eval('let a = a;');
    } catch (e) {
        write(e);
    }

    try {
        eval('const a = a;');
    } catch (e) {
        write(e);
    }

    try {
        // this works in a try/catch because Use Before Declaration is always a runtime error by the spec
        let a = a;
    } catch (e) {
        write(e);
    }

    try {
        // this works in a try/catch because Use Before Declaration is always a runtime error by the spec
        const a = a;
    } catch (e) {
        write(e);
    }
})();

(function () {
    try {
        eval('a() = 123; let a;');
    } catch (e) {
        write(e);
    }

    try {
        eval('a() = 123; const a = undefined;');
    } catch (e) {
        write(e);
    }
})();

try {
    write(delete glo_a);
} catch (e) {
    write(e);
}

try {
    write(typeof glo_a);
} catch (e) {
    write(e);
}

let glo_a = 'glo_a';

try {
    write(delete glo_a);
} catch (e) {
    write(e);
}

try {
    write(typeof glo_a);
} catch (e) {
    write(e);
}

try {
    write(delete glo_b);
} catch (e) {
    write(e);
}

try {
    write(typeof glo_b);
} catch (e) {
    write(e);
}

const glo_b = 'glo_b';

try {
    write(delete glo_b);
} catch (e) {
    write(e);
}

try {
    write(typeof glo_b);
} catch (e) {
    write(e);
}

//BLUE 404930
try {
    switch (Math.round(.2)) {
    case 3:
        let x = eval("");
    default:
        x; // should emit a use-before-decl runtime error
    }
} catch (e) {
    write(e);
}

// BLUE 404930 (bug reused, different from above)
function testStSlotNoThrow() { let y = function () { write(y = 123); }; write(y); y(); write(y); }
function testStSlotThrow() { let y = (function () { write(y = 123); })(); write(y); }

function testStObjSlotNoThrow() { let y = function () { write(y = 123); }; write(y); y(); write(y); eval('y'); }
function testStObjSlotThrow() { let y = (function () { write(y = 123); })(); write(y); eval('y'); }

function testTypeOfNoThrow() { let y = function () { write(typeof y); }; y(); }
function testTypeOfThrow() { let y = (function () { write(typeof y); })(); }

function testTypeOfObjNoThrow() { let y = function () { write(typeof y); }; y(); eval('y'); }
function testTypeOfObjThrow() { let y = (function () { write(typeof y); })(); eval('y'); }

function testLdSlotNoThrow() { let y = function () { write(y); }; y(); }
function testLdSlotThrow() { let y = (function () { write(y); })(); }

function testLdObjSlotNoThrow() { let y = function () { write(y); }; y(); eval('y'); }
function testLdObjSlotThrow() { let y = (function () { write(y); })(); eval('y'); }

try { testStSlotNoThrow(); } catch (e) { write("shouldn't throw! " + e); }
try { testStSlotThrow(); } catch (e) { write(e); }

try { testStObjSlotNoThrow(); } catch (e) { write("shouldn't throw! " + e); }
try { testStObjSlotThrow(); } catch (e) { write(e); }

try { testTypeOfNoThrow(); } catch (e) { write("shouldn't throw! " + e); }
try { testTypeOfThrow(); } catch (e) { write(e); }

try { testTypeOfObjNoThrow(); } catch (e) { write("shouldn't throw! " + e); }
try { testTypeOfObjThrow(); } catch (e) { write(e); }

try { testLdSlotNoThrow(); } catch (e) { write("shouldn't throw! " + e); }
try { testLdSlotThrow(); } catch (e) { write(e); }

try { testLdObjSlotNoThrow(); } catch (e) { write("shouldn't throw! " + e); }
try { testLdObjSlotThrow(); } catch (e) { write(e); }

function testBugVsoOs849056() {
    let x; // this x should get a scope slot
    function inner() {
        y = x; // should throw use before declaration
        let y;
    }
    inner();
}

try { testBugVsoOs849056(); } catch (e) { write(e); }

function testBugVsoOs1141661() {
    y = 5;
    let y;
    eval("write('This should be unreachable');");
}

try { testBugVsoOs1141661(); } catch (e) { write(e); }

