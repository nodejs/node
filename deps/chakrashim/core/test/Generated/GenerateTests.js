//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v); }

function foo() {}

var all = [ "undefined", "null",
            "true", "false",
            "Boolean(true)", "Boolean(false)",
            "new Boolean(true)", "new Boolean(false)",
            "NaN", "+0", "-0", "0", "0.0", "-0.0", "+0.0",            
            "1", "10", "10.0", "10.1", "-1", 
            "-10", "-10.0", "-10.1",
            "Number.MAX_VALUE", "Number.MIN_VALUE", "Number.NaN", "Number.POSITIVE_INFINITY", "Number.NEGATIVE_INFINITY",
            "new Number(NaN)", "new Number(+0)", "new Number(-0)", "new Number(0)", 
            "new Number(0.0)", "new Number(-0.0)", "new Number(+0.0)", 
            "new Number(1)", "new Number(10)", "new Number(10.0)", "new Number(10.1)", "new Number(-1)", 
            "new Number(-10)", "new Number(-10.0)", "new Number(-10.1)",
            "new Number(Number.MAX_VALUE)", "new Number(Number.MIN_VALUE)", "new Number(Number.NaN)", 
            "new Number(Number.POSITIVE_INFINITY)", "new Number(Number.NEGATIVE_INFINITY)",
            "''", "0xa", "04", "'hello'", "'hel' + 'lo'",
            "String('')", "String('hello')", "String('h' + 'ello')",
            "new String('')", "new String('hello')", "new String('he' + 'llo')",
            "new Object()", "new Object()",
            "[1, 2, 3]", "[1 ,2 , 3]",
            "new Array(3)", "Array(3)", "new Array(1 ,2 ,3)", "Array(1)",
            "foo"
          ];

var biops = [    
    ["*", "mul" ], ["/", "div"], ["%", "mod"],                  // 11.5 Multiplicative operators    
    ["+", "add" ], ["-", "sub"],                                // 11.6 Addtitive operators
    ["<<","lsh" ], [">>","rsh"], [">>>", "rshu"],               // 11.7 Bitwise shift operators
    ["<", "lt"  ], [">", "gt" ], ["<=", "le"],   [">=", "ge" ], // 11.8 Relational operators
    ["==","eq"  ], ["!=","ne" ], ["===", "seq"], ["!==","sne"], // 11.9 Equality operators
    ["&", "and" ], ["^", "xor"], ["|", "or"],                   // 11.10 Binary bitwise operators
    ["&&","land"], ["||","lor"]                                 // 11.11 Binary logical operators    
];


var fileSystemObject = new ActiveXObject("Scripting.FileSystemObject");
var WShell = new ActiveXObject("WScript.Shell");
var MaxTestsPerFile = 1000;
var fp;
var fileArray = [];

function AddCustomTests(arr)
{
    arr.push("imul");
    arr.push("divbypow2");
    arr.push("B9AA6BBF.0");
    arr.push("6E55FA7A.0");
    arr.push("attackoftheclones");
}

function OpenFileAndWriteHeader(name) {
    if (fp)
        fp.close();
    fp = fileSystemObject.CreateTextFile(name + ".js", true);
    fp.WriteLine("//-------------------------------------------------------------------------------------------------------");
    fp.WriteLine("// Copyright (C) Microsoft. All rights reserved.");
    fp.WriteLine("// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.");
    fp.WriteLine("//-------------------------------------------------------------------------------------------------------");
    fp.WriteLine("");

    fp.WriteLine('function write(v) { WScript.Echo(v + ""); }');
    fp.WriteLine("");
    fp.WriteLine("function foo() {}");
    fp.WriteLine("");

    fileArray.push(name);

}

// Generate test files
function GenerateTests() {
    for (var op = 0; op < biops.length; op++) {
        var fileNo = 0;
        OpenFileAndWriteHeader(biops[op][1]);
        var testCounter = 0;
        for (var i=0; i<all.length; ++i) {
            for (var j=0; j<all.length; ++j) {
                if(++testCounter > MaxTestsPerFile)
                {
                    OpenFileAndWriteHeader("" + biops[op][1] + (fileNo++));
                    testCounter = 0;
                }
                fp.WriteLine("write(" + all[i] + " " + biops[op][0] + " " + all[j] + ");");
            }
        }
    }
    if(fp)
        fp.close();
}
    
// Generate baseline files
function Generate_CreateBaseLine() {
    fp = fileSystemObject.CreateTextFile("createBaseLine.bat", true);
    for (var file in fileArray) {
        fp.WriteLine("cscript.exe //nologo " + fileArray[file] + ".js > " + fileArray[file] + ".baseline");
    }
    fp.close();
}

// Generate rlexe.xml files
function Generate_RLexe() {
    fp = fileSystemObject.CreateTextFile("rlexe.xml", true);

    fp.WriteLine('<?xml version="1.0"?>');
    fp.WriteLine('<regress-exe>');

    for (var file in fileArray) {
        fp.WriteLine('  <test>');
        fp.WriteLine('    <default>');
        fp.WriteLine('      <files>' + fileArray[file] + '.js</files>');
        fp.WriteLine('      <baseline>' + fileArray[file] + '.baseline</baseline>');
        fp.WriteLine('    </default>');
        fp.WriteLine('  </test>');
    }

    fp.WriteLine('</regress-exe>');
    fp.close();
}

GenerateTests();
Generate_CreateBaseLine();
AddCustomTests(fileArray);
Generate_RLexe();
