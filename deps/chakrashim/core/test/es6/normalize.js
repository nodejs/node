//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

Object.defineProperty(Object.getPrototypeOf({}), "echo", { value: function () { WScript.Echo(this); } });
function AssertEqual(actual, expected, msg) { ((actual === expected ? "Passed! " : "Failed (actual: " + actual + ", expected: " + expected + "). Message: ") + msg).echo(); };
Object.defineProperty(Object.getPrototypeOf({}), "equalTo", { value: function (other, msg) { AssertEqual(this.constructor(this), other, msg); } });

try {
    String.prototype.normalize.call("", "asd");
} catch (e) {
    e.echo();
}

try {
    String.prototype.normalize.call();
} catch (e) {
    e.echo();
}
try {
    String.prototype.normalize.call(null);
} catch (e) {
    e.echo();
}
try {
    String.prototype.normalize.call(undefined);
} catch (e) {
    e.echo();
}
try {
    new String.prototype.normalize();
} catch (e) {
    e.echo();
}

"".normalize().equalTo("", "Empty string noramlized to empty string.");

"\u00C4ffin".normalize("NFD").equalTo("A\u0308ffin", "NFD normalization test.");
"\u00C4\uFB03n".normalize("NFD").equalTo("A\u0308\uFB03n", "NFD normalization test.");
"Henry IV".normalize("NFD").equalTo("Henry IV", "NFD normalization test.");
"Henry \u2163".normalize("NFD").equalTo("Henry \u2163", "NFD normalization test.");

"\u00C4ffin".normalize("NFC").equalTo("\u00C4ffin", "NFC normalization test.");
"\u00C4\uFB03n".normalize("NFC").equalTo("\u00C4\uFB03n", "NFC normalization test.");
"Henry IV".normalize("NFC").equalTo("Henry IV", "NFC normalization test.");
"Henry \u2163".normalize("NFC").equalTo("Henry \u2163", "NFC normalization test.");

"\u00C4ffin".normalize("NFKD").equalTo("A\u0308ffin", "NFKD normalization test.");
"\u00C4\uFB03n".normalize("NFKD").equalTo("A\u0308ffin", "NFKD normalization test.");
"Henry IV".normalize("NFKD").equalTo("Henry IV", "NFKD normalization test.");
"Henry \u2163".normalize("NFKD").equalTo("Henry IV", "NFKD normalization test.");

"\u00C4ffin".normalize("NFKC").equalTo("\u00C4ffin", "NFKC normalization test.");
"\u00C4\uFB03n".normalize("NFKC").equalTo("\u00C4ffin", "NFKC normalization test.");
"Henry IV".normalize("NFKC").equalTo("Henry IV", "NFKC normalization test.");
"Henry \u2163".normalize("NFKC").equalTo("Henry IV", "NFKC normalization test.");

try {
    "a\uDC00b".normalize();
} catch (e) {
    e.echo();
}

try {
    "a\uD800b".normalize();
} catch (e) {
    e.echo();
}

String.prototype.normalize.call(5).equalTo("5", "Calling on a number object instead of string object.");