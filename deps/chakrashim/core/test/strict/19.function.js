//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

function PrintDescriptor(name, propDesc) {
    write(name + ":configurable : " + propDesc.configurable);
    write(name + ":enumerable   : " + propDesc.enumerable);
    write(name + ":writable     : " + propDesc.writable);    
    write(name + ":getter       : " + propDesc.get);
    write(name + ":setter       : " + propDesc.set);    
    write(name + ":value        : " + propDesc.value);
}

function exceptToString(ee) {
    if (ee instanceof TypeError) return "TypeError";
    if (ee instanceof ReferenceError) return "ReferenceError";
    if (ee instanceof EvalError) return "EvalError";
    if (ee instanceof SyntaxError) return "SyntaxError";
    return "Unknown Error";
}

(function Test1() {
    var str = "function.caller get";
        
    try {
        Test1.caller;
    } catch (e) {
        write("Exception: " + str + " " + exceptToString(e));
        return;
    }
    write("Return: " + str);
})();

(function Test2() {
    var str = "function.caller set";
        
    try {
        Test2.caller = 10;
    } catch (e) {
        write("Exception: " + str + " " + exceptToString(e));
        return;
    }
    write("Return: " + str);
})();

(function Test3() {
    var str = "function.arguments get";
        
    try {
        Test3.arguments;
    } catch (e) {
        write("Exception: " + str + " " + exceptToString(e));
        return;
    }
    write("Return: " + str);
})();

(function Test4() {
    var str = "function.arguments get";
        
    try {
        Test4.arguments = 10;
    } catch (e) {
        write("Exception: " + str + " " + exceptToString(e));
        return;
    }
    write("Return: " + str);
})();

(function Test5() {
    var str = "function.caller get";
        
    try {
        PrintDescriptor("Test5.caller", Object.getOwnPropertyDescriptor(Test5, "caller"));
    } catch (e) {
        write("Exception: " + str + " " + exceptToString(e));
        return;
    }
    write("Return: " + str);
})();

(function Test6() {
    var str = "function.arguments get";
        
    try {
        PrintDescriptor("Test6.arguments", Object.getOwnPropertyDescriptor(Test6, "arguments"));
    } catch (e) {
        write("Exception: " + str + " " + exceptToString(e));
        return;
    }
    write("Return: " + str);
})();