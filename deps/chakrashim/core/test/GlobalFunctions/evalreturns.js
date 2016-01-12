//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

// for loop that produces a value
write(eval('for (var i = 0; i < 1; i++) { i; var q = "wrong"; }') + '');
// for loop that doesn't
write(eval('for (i = 0; i < 0; i++) { i; var q = "wrong"; }') + '');
// another for loop that doesn't
write(eval('for (i = 0; i < 1; i++) { var q = "wrong"; }') + '');

// if that executes nothing
write(eval('if (i = 0) { i; }') + '');
// if with a value
write(eval('if (i = 1) { i; }') + '');
// if-else with value in if
write(eval('if (i = 0) { ++i; } else { i; var q = "wrong"; }') + '');
// if-else with value in else
write(eval('if (i = 1) { ++i; } else { i; var q = "wrong"; }') + '');

// try-catch with value in try
write(eval('try { "hello"; throw 0; } catch(e) {}') + '');
// try-catch with value in try and catch
write(eval('try { "hello"; throw 0; } catch(e) {++e;}') + '');
// try-catch with no value
write(eval('try { throw 0; } catch(e) { var q = "wrong"; }') + '');
// try-catch with value in catch
write(eval('try { throw 0; } catch(e) { ++e; var q = "wrong"; }') + '');

// try-finally with value in try
write(eval('try { "hello"; } finally {}') + '');
// try-finally with value in finally
write(eval('try { "hello"; } finally {"goodbye";}') + '');
// try-finally with no value
write(eval('try {} finally { var q = "wrong"; }') + '');
// try-finally with value only in finally
write(eval('try {} finally { "good night"; var q = "wrong"; }') + '');

// Function expressions produce a value but function declarations don't
write(eval("(function () { });")); // different behavior in standards mode
write(eval("1; (function () { });")); // different behavior in standards mode
write(eval("(function () { }); 2;"));
write(eval("1; (function () { }); 2;"));
write(eval("function x() { };"));
write(eval("1; function x() { };"));
write(eval("function x() { }; 2;"));
write(eval("1; function x() { }; 2;"));

// eval of switch returns the last expression statement's value
write(eval("switch(1) { case 2: 3; break; case 1: 2; break; default: 7; break }"));

// eval of a comma expression
function ret_false(){return false;}
function ret_true(){return true;}
write(eval("ret_true(),ret_false();"));

try {
    // post-increment a function result (should throw)
    write(eval("++({ f: function() { } }.f());"));
}
catch(e) {
    write(e.message);
}

write(eval.call());
write(eval.call('test'));
write(eval.call('test1', '"test2"'));