//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Try variants of a pattern known to cause exploitable trashing of the Null
// object's vtable: access a local var/function only within the non-evaluated
// branch of a ?: operator, then do instanceof null to force virtual call using
// the Null object's vtable.

function write(x) { WScript.Echo(x + ''); }

(function () {
    (function () {
        return true ? true : x;
    })();
    function x() { }; 
})();
try {
    var z = Object instanceof null;
}
catch (e) {
    write(e.message);
}

(function () {
    (function () {
        return true ? true : x;
    })();
    var x;
})();
try {
    var z = Object instanceof null;
}
catch (e) {
    write(e.message);
}

(function () {
    (function () {
        return false ? x : false;
    })();
    function x() { }; 
})();
try {
    var z = Object instanceof null;
}
catch (e) {
    write(e.message);
}

(function () {
    (function () {
        return false ? x : false;
    })();
    var x;
})();
try {
    var z = Object instanceof null;
}
catch (e) {
    write(e.message);
}
