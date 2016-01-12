//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(x) { WScript.Echo(x + ""); }

(function f(x) {
    write(f);
    write(x);
    (function () {
        write(f);
        write(x);
        eval('f = "inner f";');
        eval('x = "inner x";');
        write(f);
        write(x);
        eval('var f = "inner f 2";');
        eval('var x = "inner x 2";');
        write(f);
        write(x);
    })();
    write(f);
    write(x);
})('outer x');

var functest;
var vartest = 0;
var value = (function functest(arg) {
    eval('');
    if (arg) return 1;
    vartest = 1;
    functest = function(arg) {
        return 2;
    }; // this line does nothing as 'functest' is ReadOnly here
    return functest(true); // this is therefore tail recursion and returns 1
})(false);
WScript.Echo('vartest = ' + vartest);
WScript.Echo('value = ' + value);

(function (){
    try {
        throw 'hello';
    }
    catch(e) {
        var f = function(){ eval('WScript.Echo(e)') };
    }
    f();
})();

var moobah = function moobah() {
    this.innerfb = function() {
        moobah.x = 'whatever';
    }
    this.innerfb();
    write(moobah.x);
}

moobah();
