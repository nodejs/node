//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// FIXME:
// The diagnostic produced by getters is correct, but the setters
// issue assertion failures and other sorts of odd behavior.
// This problem should be fixed, the commented-out calls to obj[...] = {}
// should be restored, and this test should be rebaselined.

var echo = WScript.Echo;

var diag = function() {
    echo("  Type: " + (typeof this));
    echo("    Is Object:  " + (this instanceof Object));
    echo("    Is Number:  " + (this instanceof Number));
    echo("    Is Boolean: " + (this instanceof Boolean));
    echo("    Is String:  " + (this instanceof String));
}

var diag_strict = function() {
    "use strict";
    echo("  Type: " + (typeof this));
    echo("    Is Object:  " + (this instanceof Object));
    echo("    Is Number:  " + (this instanceof Number));
    echo("    Is Boolean: " + (this instanceof Boolean));
    echo("    Is String:  " + (this instanceof String));
}

var getDiag = function() {
    echo("Executing getter:");
    diag.apply(this, arguments);
    return true;
}

var getDiag_strict = function() {
    "use strict";
    echo("Executing getter:");
    diag_strict.apply(this, arguments);
    return true;
}

var setDiag = function(x) {
    echo("Executing setter:");
    diag.apply(this, arguments);
}

var setDiag_strict = function(x) {
    "use strict";
    echo("Executing setter:");
    diag_strict.apply(this, arguments);
}

var funcDiag = function(x) {
    echo("Executing function:");
    diag.apply(this, arguments);
    return function () {
        echo("  ... returning from function");
    }
}

var funcDiag_strict = function(x) {
    "use strict";
    echo("Executing function:");
    diag_strict.apply(this, arguments);
    return function () {
        echo("  ... returning from function");
    }
}

Object.defineProperties(
    Number.prototype, {
        "foo": {
            get: getDiag,
            //set: setDiag
        },
        "foo_strict": {
            get: getDiag_strict,
            //set: setDiag_strict
        },
        "bar": {
            get: funcDiag
        },
        "bar_strict": {
            get: funcDiag_strict
        },
        "42": {
            get: getDiag,
            //set: setDiag
        },
        "142": {
            // 142 means "42_strict"
            get: getDiag_strict,
            //set: setDiag_strict
        },
        "43": {
            get: funcDiag
        },
        "143": {
            // 143 means "43_strict"
            get: funcDiag_strict
        },
        "-42": {
            get: getDiag,
            //set: setDiag
        },
        "-142": {
            // -142 means "-42_strict"
            get: getDiag_strict,
            //set: setDiag_strict
        },
        "-43": {
            get: funcDiag
        },
        "-143": {
            // -143 means "-43_strict"
            get: funcDiag_strict
        }
    });

Object.defineProperties(
    Boolean.prototype, {
        "foo": {
            get: getDiag,
            //set: setDiag
        },
        "foo_strict": {
            get: getDiag_strict,
            //set: setDiag_strict
        },
        "bar": {
            get: funcDiag
        },
        "bar_strict": {
            get: funcDiag_strict
        },
        "42": {
            get: getDiag,
            //set: setDiag
        },
        "142": {
            // 142 means "42_strict"
            get: getDiag_strict,
            //set: setDiag_strict
        },
        "43": {
            get: funcDiag
        },
        "143": {
            // 143 means "43_strict"
            get: funcDiag_strict
        },
        "-42": {
            get: getDiag,
            //set: setDiag
        },
        "-142": {
            // -142 means "-42_strict"
            get: getDiag_strict,
            //set: setDiag_strict
        },
        "-43": {
            get: funcDiag
        },
        "-143": {
            // -143 means "-43_strict"
            get: funcDiag_strict
        }
    });

Object.defineProperties(
    String.prototype, {
        "foo": {
            get: getDiag,
            //set: setDiag
        },
        "foo_strict": {
            get: getDiag_strict,
            //set: setDiag_strict
        },
        "bar": {
            get: funcDiag
        },
        "bar_strict": {
            get: funcDiag_strict
        },
        "42": {
            get: getDiag,
            //set: setDiag
        },
        "142": {
            // 142 means "42_strict"
            get: getDiag_strict,
            //set: setDiag_strict
        },
        "43": {
            get: funcDiag
        },
        "143": {
            // 143 means "43_strict"
            get: funcDiag_strict
        },
        "-42": {
            get: getDiag,
            //set: setDiag
        },
        "-142": {
            // -142 means "-42_strict"
            get: getDiag_strict,
            //set: setDiag_strict
        },
        "-43": {
            get: funcDiag
        },
        "-143": {
            // -143 means "-43_strict"
            get: funcDiag_strict
        }
    });

var runTests = function(obj, objKind) {
    var f = "f";
    var b = "b";

    echo("** Testing " + objKind + ", property 'foo' (value) **");
    obj.foo;
    obj[f + "oo"];
    typeof obj.foo;
    obj.foo instanceof Object;
//  obj.foo = {};
    echo("");

    echo("** Testing " + objKind + ", property 'bar' (function) **");
    obj.bar();
    obj[b + "ar"]();
    typeof obj.bar();
    obj.bar() instanceof Object;
    echo("");

    echo("** Testing " + objKind + ", property 42 (value) **");
    obj[42];
    obj[41 + 1];
    typeof obj[42];
    obj[42] instanceof Object;
//  obj[42] = {};
    echo("");

    echo("** Testing " + objKind + ", property 43 (function) **");
    obj[43]();
    obj[45 - 2]();
    typeof obj[43]();
    obj[43]() instanceof Object;
    echo("");

    echo("** Testing " + objKind + ", property -42 (value) **");
    obj[-42];
    obj[-41 - 1];
    typeof obj[-42];
    obj[-42] instanceof Object;
//  obj[-42] = {};
    echo("");

    echo("** Testing " + objKind + ", property -43 (function) **");
    obj[-43]();
    obj[-45 + 2]();
    typeof obj[-43]();
    obj[-43]() instanceof Object;
    echo("");
}

var runTests_strict = function(obj, objKind) {
    var f = "f";
    var b = "b";

    echo("** Testing " + objKind + ", property 'foo_strict' (value, strict mode) **");
    obj.foo_strict;
    obj[f + "oo_strict"];
    typeof obj.foo_strict;
    obj.foo_strict instanceof Object;
//  obj.foo_strict = {};
    echo("");

    echo("** Testing " + objKind + ", property 'bar_strict' (function, strict mode) **");
    obj.bar_strict();
    obj[b + "ar_strict"]();
    typeof obj.bar_strict();
    obj.bar_strict() instanceof Object;
    echo("");

    echo("** Testing " + objKind + ", property 142 (value, strict mode) **");
    obj[142];
    obj[141 + 1];
    typeof obj[142];
    obj[142] instanceof Object;
//  obj[142] = {};
    echo("");

    echo("** Testing " + objKind + ", property 143 (function, strict mode) **");
    obj[143]();
    obj[145 - 2]();
    typeof obj[143]();
    obj[143]() instanceof Object;
    echo("");

    echo("** Testing " + objKind + ", property -142 (value, strict mode) **");
    obj[-142];
    obj[-141 - 1];
    typeof obj[-142];
    obj[-142] instanceof Object;
//  obj[-142] = {};
    echo("");

    echo("** Testing " + objKind + ", property -143 (function, strict mode) **");
    obj[-143]();
    obj[-145 + 2]();
    typeof obj[-143]();
    obj[-143]() instanceof Object;
    echo("");
}

var i = 3;
runTests(i, "int");
runTests_strict(i, "int");

var l = (1 << 30) + 1;
runTests(l, "large int");
runTests_strict(l, "large int");

var d = 3.14;
runTests(d, "float");
runTests_strict(d, "float");

var b = true;
runTests(b, "bool");
runTests_strict(b, "bool");

var s = "Hello";
runTests(s, "string");
runTests_strict(s, "string");
