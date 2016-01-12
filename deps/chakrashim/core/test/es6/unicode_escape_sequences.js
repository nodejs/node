//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

Object.defineProperty(Object.getPrototypeOf({}), "echo", { value: function () { WScript.Echo(this); } });
Object.defineProperty(Object.getPrototypeOf({}), "echos", { value: function () { WScript.Echo(JSON.stringify(this)); } });
Object.defineProperty(Object.getPrototypeOf({}), "echoChars", {
    value: function () {
        var chars = [];
        var that = String(this);
        for (var i = 0; i < that.length; i++) {
            chars.push(that.charCodeAt(i));
        }
        chars.echos();
    }
});
Object.defineProperty(Object.getPrototypeOf({}), "echoCP", {
    value: function () {
        var chars = [];
        var that = String(this);
        for (var i = 0; i < that.length;) {
            var cp = that.codePointAt(i);
            chars.push(cp);
            i += cp > 0x10000 ? 2 : 1;
        }
        chars.echos();
    }
});

// Exceptions expected
try {
    eval('/./\\u0069'); //Bug 165551; before this was allowed because \u0069 is 'i'
} catch (e) {
    e.echo();
}
try {
    eval('var test = "\\u{0}"'); //Valid 11/20/2013
} catch (e) {
    e.echo();
}
try {
    eval('var test = "\\u{00}"'); //Valid 11/20/2013
} catch (e) {
    e.echo();
}

try {
    eval('var test = "\\u{000}"');//Valid 11/20/2013
} catch (e) {
    e.echo();
}

try {
    eval('var test = "\\u{0000000}"');//Valid 11/20/2013
} catch (e) {
    e.echo();
}

try {
    eval('var test = "\\u{200000}"'); 
} catch (e) {
    e.echo();
}

try {
    new RegExp("[\u{0}-\u{1}]"); //Valid 11/20/2013
} catch (e) {
    e.echo();
}

try {
    new RegExp("[\u{10000}-\u{10001}]"); 
} catch (e) {
    e.echo();
}

try {
    new RegExp("[\u{10000}-\u{10001}]", "u"); 
} catch (e) {
    e.echo();
}

// Shouldn't throw From here onwards.
eval('var test = "\\u{0000}"');

// Regex doesn't throw in these cases, however, it will try matching \u and then {1} instead of the actual value of \u0001.
// Should print false(11/11/2014) '/u' option not suplied
/\u{1}/.test("\u0001").echo();
/\u{01}/.test("\u0001").echo();
/\u{001}/.test("\u0001").echo();
/\u{0000001}/.test("\u0001").echo();

/\u{1}/.test("\u{0001}").echo();
/\u{01}/.test("\u{0001}").echo();
/\u{001}/.test("\u{0001}").echo();
/\u{0000001}/.test("\u{0001}").echo();

// Should print true
/\u{0001}/.test("\u0001").echo();
/\u{00001}/.test("\u0001").echo();
// Should print true
/\u{0001}/.test("\u{0001}").echo();
/\u{00001}/.test("\u{0001}").echo();

try{
    new RegExp("[\\u{1000}-\\u{1001}]").test("\u{1001}").echo();
} catch (ex) {
    ex.echo();
}
try{
    eval("/[\\u{1000}-\\u{1001}]/").test("\u{1001}").echo(); //invalid (11/11/2014)
} catch (ex) {
    ex.echo();
}

//This should print true
/\u{1}/u.test("\u0001").echo();
/\u{01}/u.test("\u0001").echo();
/\u{001}/u.test("\u0001").echo();
/\u{0000001}/u.test("\u0001").echo();

/\u{1}/u.test("\u{0001}").echo();
/\u{01}/u.test("\u{0001}").echo();
/\u{001}/u.test("\u{0001}").echo();
/\u{0000001}/u.test("\u{0001}").echo();

// Should print true
/\u{0001}/u.test("\u0001").echo();
/\u{00001}/u.test("\u0001").echo();
// Should print true
/\u{0001}/u.test("\u{0001}").echo();
/\u{00001}/u.test("\u{0001}").echo();

////Valid
var validStrings = [ "\u{1}", "\u{10}", "\u{100}", "\u{1000}", "\u{10000}", "\u{100000}", "\u{10FFFF}", "\u{00000001000}", "\u{00000000000000000000000000000}"];
var validIDs = [ "\u{41}", "\u{0041}", "\u{00041}", "\u{20BB7}", "\u{0000000020BB7}" ];
validStrings.forEach(function (str) {
    eval("/" + str + "/.test('" + str + "').echo()");
});

validIDs.forEach(function (str) {
    eval("/" + str + "/.test('" + str + "').echo()");
});

var invalidStrings = ["\\u\{\}", "\\u\{1000000000\}", "\\u\{110000\}"];

invalidStrings.forEach(function (str){
    try{
        eval(invalidStrings + ".echo()");
    } catch(ex){
        ex.echo();
    }
});

/a\u{}b/.test("au\{\}b").echo();
/a\u{1}b/.test("a\u0001b").echo();
/a\u{1.1}b/.test("au\{1.1\}b").echo();
/a\u{110000}b/.test("a" + (Array(110001).join('u')) +"b").echo();
/a\u{11FFFF}b/.test("au\{11FFFF\}b").echo();
/a\u{10FFFF}b/.test("a\uDBFF\uDFFFb").echo();

/a\u{1000000}b/.test("au\{1000000\}b").echo();
/a\u{1.1}b/.test("a\u0001b").echo();
/a\u{1}b/.test("a\\ub").echo();
/a\u{1}b/.test("aub").echo();
/a\u{}b/.test("a\u{0}b").echo();

/a\u{}b/u.test("au\{\}b").echo();
/a\u{1}b/u.test("a\u0001b").echo();
/a\u{1.1}b/u.test("au\{1.1\}b").echo();
/a\u{110000}b/u.test("a" + (Array(110001).join('u')) +"b").echo();
/a\u{11FFFF}b/u.test("au\{11FFFF\}b").echo();
/a\u{10FFFF}b/.test("a\uDBFF\uDFFFb").echo();

/a\u{1000000}b/u.test("au\{1000000\}b").echo();
/a\u{1.1}b/u.test("a\u0001b").echo();
/a\u{1}b/u.test("a\\ub").echo();
/a\u{1}b/u.test("aub").echo();
/a\u{}b/u.test("a\u{0}b").echo();


try {
  eval("var do\u0061 = 5;");//Shouldn't throw a syntax error
} catch(e) {
  e.echo();
}