//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function generateTestFunctionAndCall(
    functionTypeAndName,            // integer: 0 is declaration, 1 is declaration named 'arguments', 2 is unnamed expression, 3 is expression named 'arguments'
    parameterArguments,             // boolean
    nestedFunctionType,             // integer: 0 is no nested function, 1 is nested function declaration, 2 is nested function expression
    varDeclarationArgumentsType,    // integer: 0 is no var declaration, 1 is var declaration, 2 is initialized var declaration
    callsEval)                      // boolean
{
    var typeMap =
    {
        "function": "function",
        "number": "parameter",
        "string": "var",
        "object": "arguments"
    };

    // The generated function will look something like this:
    //    safeCall(function ()
    //    {
    //        function arguments(arguments)
    //        {
    //            eval("");
    //            (function arguments() { });
    //            var arguments = "hi";
    //            writeLine(typeMap[typeof (arguments)]);
    //        }
    //        arguments(1);
    //    });

    var functionCode = "";
    switch (functionTypeAndName)
    {
        case 2:
        case 3:
            functionCode += "(";
    }
    functionCode += "function";
    var functionName = "";
    switch (functionTypeAndName)
    {
        case 0:
            functionName = "foo";
            break;
        case 1:
        case 3:
            functionName = "arguments";
    }
    functionCode += " " + functionName + "(";

    if (parameterArguments)
        functionCode += "arguments";
    functionCode += "){";

    if (callsEval)
        functionCode += 'eval("");';

    switch (nestedFunctionType)
    {
        case 1:
            functionCode += "function arguments(){}";
            break;
        case 2:
            functionCode += "(function arguments(){});";
    }

    switch (varDeclarationArgumentsType)
    {
        case 1:
        case 2:
            functionCode += "var arguments";
            if (varDeclarationArgumentsType === 2)
                functionCode += '="hi"';
            functionCode += ";";
    }

    functionCode += "writeLine(typeMap[typeof(arguments)]);}";
    switch (functionTypeAndName)
    {
        case 0:
        case 1:
            functionCode += functionName;
            break;
        case 2:
        case 3:
            functionCode += ")";
    }
    functionCode += "(1);";

    writeLine(functionCode);
    eval("safeCall(function(){" + functionCode + "});");
}

var booleans = [false, true];
for (var functionTypeAndName = 0; functionTypeAndName <= 3; ++functionTypeAndName)
{
    for (var parameterArgumentsIndex in booleans)
    {
        for (var nestedFunctionType = 0; nestedFunctionType <= 2; ++nestedFunctionType)
        {
            for (var varDeclarationArgumentsType = 0; varDeclarationArgumentsType <= 2; ++varDeclarationArgumentsType)
            {
                for (var callsEvalIndex in booleans)
                {
                    var parameterArguments = booleans[parameterArgumentsIndex];
                    var callsEval = booleans[callsEvalIndex];

                    generateTestFunctionAndCall(
                        functionTypeAndName,
                        parameterArguments,
                        nestedFunctionType,
                        varDeclarationArgumentsType,
                        callsEval);
                }
            }
        }
    }
}

// Helpers

function writeLine(str)
{
    WScript.Echo("" + str);
}

function safeCall(func)
{
    try
    {
        return func();
    }
    catch (ex)
    {
        writeLine(ex.name + ": " + ex.message);
    }
}
