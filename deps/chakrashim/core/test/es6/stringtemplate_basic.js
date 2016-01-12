//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function echo(str) {
        WScript.Echo(str);
}

function printArr(arr, name) {
        for(var i in arr) {
                echo(`${name}[${i}]=${arr[i]}`);
        }
}

function tag(callSiteObj, sub) {
        printArr(callSiteObj, "callsiteObj");
        printArr(callSiteObj.raw, "callsiteObj.raw");
        echo(sub);
}

function simpleTag(callSiteObj, sub1, sub2, sub3, sub4) {
        var str = '';

        for(var i in callSiteObj) {
                if(i == 'raw') {
                        continue;
                }
                str += callSiteObj[i];
                if(i==0 && sub1) {
                        str += sub1;
                } else if(i==1 && sub2) {
                        str += sub2;
                } else if(i==2 && sub3) {
                        str += sub3;
                } else if(i==3 && sub4) {
                        str += sub4;
                }
        }

        return str;
}

function simpleRawTag(callSiteObj, sub1, sub2, sub3, sub4) {
        var str = '';

        for(var i in callSiteObj.raw) {
                str += callSiteObj.raw[i];
                if(i==0 && sub1) {
                        str += sub1;
                } else if(i==1 && sub2) {
                        str += sub2;
                } else if(i==2 && sub3) {
                        str += sub3;
                } else if(i==3 && sub4) {
                        str += sub4;
                }
        }

        return str;
}

echo("Tag function takes less formal arguments than string template produces:");
tag`str1 ${'str2'} str3 ${'str4'} str5`;
tag`str1 ${'str2'} str3 ${'str4'} str5 ${'str6'} str7`;

echo("\nSimple tag function which assembles arguments:");
echo(simpleTag`str1 ${'str2'} str3 ${'str4'} str5 ${'str6'} str7 ${'str8'} str9`);

echo("\nSimple tag function which assembles raw arguments:");
echo(simpleRawTag`str1 ${'str2'} str3 ${'str4'} str5 ${'str6'} str7 ${'str8'} str9`);

function timesThirty(arg) {
        return arg * 30;
}
echo("\nSubstitution which calls a function:");
echo(`thirty = ${timesThirty(1)}. sixty = ${timesThirty(2)}.`);

echo("\nFunction which returns a template literal:");
function add(i, j) {
        return `${i + j}`;
}

echo(add(5,3));
echo(add(500, 100));
echo(add(`${add('hello'," world ")}`,1));

echo("\nSubstitution calls a built-in operator:");
echo(`typeof 1 == ${typeof 1}.`);

echo("\nNested templates with substitutions:");
function getString() { return `funcreturn`; }
echo(`this is a nested ${`(nested template ${getString()}!)`} string template... ${getString()}s!`);

echo("");

// Make sure we don't get spurious assignment tracking when incrementing a tagged template
try {
    Function('function x(){}--++ x`${x--}`;')();
}
catch(e) {
}

function getCallSite(siteObj) {
        return siteObj;
}

function verifyCallSite(siteObj, cookedVals, rawVals) {
        var success = true;
        echo("Verifying siteObj against a known set of cooked and raw values...");

        for(var i = 0; i < siteObj.length; i++) {
                if(siteObj[i] !== cookedVals[i]) {
                        echo(`FAILED! (expected = ${cookedVals[i]})`);
                        success = false;
                }
        }
        for(var i = 0; i < siteObj.raw.length; i++) {
                if(siteObj.raw[i] !== rawVals[i]) {
                        echo(`FAILED! (expected = ${rawVals[i]})`);
                        success = false;
                }
        }

        if(success) {
                echo(`PASSED
`);
        } else {
                echo(`FAILED
`);
        }
}

verifyCallSite(getCallSite`somestring`, ["somestring"], ["somestring"]);
verifyCallSite(getCallSite`\\`, ["\\"], ["\\\\"]);
verifyCallSite(getCallSite``, [""], [""]);
verifyCallSite(getCallSite`\u00b1`, ["\u00b1"], ["\\u00b1"]);
verifyCallSite(getCallSite`\ua48c`, ["\ua48c"], ["\\ua48c"]);
verifyCallSite(getCallSite`\u0023`, ["\u0023"], ["\\u0023"]);
verifyCallSite(getCallSite`\xF8`, ["\xF8"], ["\\xF8"]);
verifyCallSite(getCallSite`\h`, ["\h"], ["\\h"]);
verifyCallSite(getCallSite`\"`, ["\""], ["\\\""]);
verifyCallSite(getCallSite`\n`, ["\n"], ["\\n"]);
verifyCallSite(getCallSite`\r`, ["\r"], ["\\r"]);
verifyCallSite(getCallSite`\r\n`, ["\r\n"], ["\\r\\n"]);
verifyCallSite(getCallSite`\r\n\r\n`, ["\r\n\r\n"], ["\\r\\n\\r\\n"]);
verifyCallSite(getCallSite`\n\n\n\n\n\n\n\n\n\n`, ["\n\n\n\n\n\n\n\n\n\n"], ["\\n\\n\\n\\n\\n\\n\\n\\n\\n\\n"]);
verifyCallSite(getCallSite`$`, ["$"], ["$"]);
verifyCallSite(getCallSite`\"bunch of escape chars \v\t\n\b\a\"`,
["\"bunch of escape chars \v\t\n\b\a\""],
["\\\"bunch of escape chars \\v\\t\\n\\b\\a\\\""]);
verifyCallSite(getCallSite`rest of escape chars \b\t\f\r`,
["rest of escape chars \b\t\f\r"],
["rest of escape chars \\b\\t\\f\\r"]);

verifyCallSite(getCallSite`\
`, [""], ["\\\n"]);
verifyCallSite(getCallSite`
`, ["\n"], ["\n"]);
verifyCallSite(getCallSite``, ["\n"], ["\n"]);
verifyCallSite(getCallSite`\`, [""], ["\\\n"]);
verifyCallSite(getCallSite`
`, ["\n\n"], ["\n\n"]);
verifyCallSite(getCallSite` `, ["\u2028"], ["\u2028"]);
verifyCallSite(getCallSite` `, ["\u2029"], ["\u2029"]);
verifyCallSite(getCallSite`$ $ $ {} } { }} {{`, ["$ $ $ {} } { }} {{"], ["$ $ $ {} } { }} {{"]);

// String.raw must handle embedded null characters
function nullCharTest() {
    function escapeNullCharacters(s) {
        return (s+'').replace('\0', '\\0');
    }

    var s = String.raw`string literal pre ${'string expression pre \0 string expression post'} string literal post`;
    return escapeNullCharacters(s);
}
echo(nullCharTest());
