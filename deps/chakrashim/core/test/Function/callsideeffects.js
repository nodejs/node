//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo("" + v); }

var x = ['x'];
x.call1 = function() { write('In x.call1: this = ' + this) };

var y = ['y'];
y.call1 = function() { write('In y.call1: this = ' + this) };

function call1()
{
    write('In call1: this = ' + this);
}

function call2()
{
    write('In call2: this = ' + this);
}

var savecall1 = call1;
call1(call1 = call2);
call1 = savecall1;

savecall1 = x.call1;
x.call1(x.call1 = call1);
x.call1 = savecall1;

var savex = x;
x.call1(x.call1 = y.call1);
x = savex;
x.call1 = savecall1;

var s = 'call1';
x[x = s]();
x = savex;

x[s](s = 'call2');
s = 'call1';

x[s](x[s] = y.call1);
x[s] = x.call1;

(function() {
    // Now try the same set of stuff when call target components are local.

    var x = ['x'];
    x.call1 = function() { write('In x.call1: this = ' + this) };

    var y = ['y'];
    y.call1 = function() { write('In y.call1: this = ' + this) };

    function call1()
    {
        write('In call1: this = ' + this);
    }

    function call2()
    {
        write('In call2: this = ' + this);
    }

    var savecall1 = call1;
    call1(call1 = call2);
    call1 = savecall1;

    savecall1 = x.call1;
    x.call1(x.call1 = call1);
    x.call1 = savecall1;

    var savex = x;
    x.call1(x.call1 = y.call1);
    x = savex;
    x.call1 = savecall1;

    var s = 'call1';
    x[x = s]();
    x = savex;

    x[s](s = 'call2');
    s = 'call1';

    x[s](x[s] = y.call1);
    x[s] = x.call1;
})();

(function() {
var evalExpr = '' +
    'var x = ["x"];' +
    'x.call1 = function() { write("In x.call1: this = " + this) };' +

    'var y = ["y"];' +
    'y.call1 = function() { write("In y.call1: this = " + this) };' +

    'function call1()' +
    '{' +
        'write("In call1: this = " + this);' +
    '}' +

    'function call2()' +
    '{' +
        'write("In call2: this = " + this);' +
    '}' +

    'var savecall1 = call1;' +
    'call1(call1 = call2);' +
    'call1 = savecall1;' +

    'savecall1 = x.call1;' +
    'x.call1(x.call1 = call1);' +
    'x.call1 = savecall1;' +

    'var savex = x;' +
    'x.call1(x.call1 = y.call1);' +
    'x = savex;' +
    'x.call1 = savecall1;' +

    'var s = "call1";' +
    'x[x = s]();' +
    'x = savex;' +

    'x[s](s = "call2");' +
    's = "call1";' +

    'x[s](x[s] = y.call1);' +
    'x[s] = x.call1;';

    eval(evalExpr);
})();

// Verify that we assign regs properly in a compound assignment to function call result.
// Note: this will cease to be a valid test when early reference errors are thrown by default.
function test5921858() {
    function eval([]){}
    function shapeyConstructor(fujzty){
        Object.defineProperty(this, "a", 
                              ({value: ((eval("true", window)) ^= z), writable: true, configurable: false}));
    }
}
