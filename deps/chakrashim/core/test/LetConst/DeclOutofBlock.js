//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Tests for ERRDeclOutOfBlock "Const and let must be declared inside of block"
var a = 1;

(function () {
    try { eval(
        "if (a) \
            let b = 5;" // error
    );} catch (e) { WScript.Echo(e); }
    try { eval(
        "if (a) \
            const b = 5;" // error
    );} catch (e) { WScript.Echo(e); }
})();

(function () {
    try { eval(
    "if (a) { \
        let c = 3; /* no error */ \
        const x = 42; /* no error */ \
    }"
    );} catch (e) { WScript.Echo(e); }
})();

(function () {
    try { eval(
    "while (a) \
         let d = 5;" // error
    );} catch (e) { WScript.Echo(e); }
    try { eval(
    "while (a) \
         let d = 5;" // error
    );} catch (e) { WScript.Echo(e); }
})();

(function () {
    try { eval(
    "while (a) { \
        let e = 10; /* no error */ \
        const y = 10; /* no error */ \
        break; \
    }"
    );} catch (e) { WScript.Echo(e); }
})();

(function () {
    try { eval(
    "if (a) \
        while (a) \
            if (a) { \
                let x = 3; /* no error */ \
                const z = x; /* no error */ \
                while (a) \
                    let f = 5; /* error */ \
            }"
    );} catch (e) { WScript.Echo(e); }
})();

function test() {
    if (a)
        for (let x in [1]) {    /* no error */
            break;
        };

    for (var y in [1])
        for (let x in [1]) {    /* no error */
            break;
        };

    do
        for (let x in [1]) {    /* no error */
            break;
        }
    while (!a);

    if (a)
        for (let x = 0; x < 1; x++) {    /* no error */
            break;
        };

    for (var y in [1])
        for (let x = 0; x < 1; x++) {    /* no error */
            break;
        };

    do
        for (let x = 0; x < 1; x++) {    /* no error */
            break;
        }
    while (!a);

    WScript.Echo('success');
};
test();

