//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function Execute(str)
{
    try {
        eval(str);
    }
    catch (e) {
        WScript.Echo(e);
    }
}

// Keywords that are reserved cannot be used as identifiers. Examples: var, with, false
// Other keywords are not reserved, and can be used as identifiers. Examples: double, byte

// regular identifier, referenced later with unicode variant
Execute("var hello=10; WScript.Echo(hello); WScript.Echo(h\\u0065llo);");
// identifier with unicode variant. Used with and without unicode
Execute("var h\u0065llo=20; WScript.Echo(hello); WScript.Echo(h\\u0065llo);");
// undefined identifier with unicode. Should throw a reference error
Execute("WScript.Echo(h\\u0065llo2);");

// Valid use of some reserved keywords in expressions, like FALSE
// Invalid as of ES6 Draft 22
Execute("WScript.Echo(fals\\u0065);");
Execute("var a = fals\\u0065; WScript.Echo(a);");
Execute("var b = tru\\u0065; WScript.Echo(b);");

// Invalid use of a reserved keyword in an expression. Should throw a syntax error
Execute("var c = var;");
Execute("var c = v\\u0061r;");
Execute("var c = else;");
Execute("var c = els\\u0065;");

// Reserved keyword declared as a var. Should throw an error indicating use of keyword as an identifier
Execute("var false=30; WScript.Echo(false); WScript.Echo(fals\\u0065);");
Execute("var var=30; WScript.Echo(var); WScript.Echo(v\\u0061r);");
Execute("var fals\\u0065=40; WScript.Echo(false); WScript.Echo(fals\\u0065);");
Execute("var v\\u0061r=30; WScript.Echo(var); WScript.Echo(v\\u0061r);");

// Use a reserved keyword as a property, legal
Execute("var x1={};x1.else = 10;WScript.Echo(x1.else);");
Execute("var x2={};x2.els\\u0065 = 10;WScript.Echo(x2.els\\u0065);");

// Use an unreserved keyword as a property, legal
Execute("var x1={};x1.double = 10;WScript.Echo(x1.double);");
Execute("var x2={};x2.doubl\\u0065 = 10;WScript.Echo(x2.doubl\\u0065);");

// Use a reserved keyword as a function name, not legal
Execute("function else() {};");
Execute("function els\\u0065() {};");

// Use an unreserved keyword as a function name, legal
Execute("function double() {};");
Execute("function doubl\\u0065() {};");
