//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

function global() { write("global"); }
function another() { write("another"); }
function g1() { write("g1"); }
function g2() { write("g2"); }
function g3() { write("g3"); }
function g4() { write("g4"); }

(function () {
    g1();
    var x = function g1() { write("first"); } 
    g1();
    var y = function g1() { write("second"); }
    g1();
})();


(function () {
    try { g2(); } catch (e) { write(e); }

    var g2 = global;
    try { g2(); } catch (e) { write(e); }

    var y = function g2() { write("second"); }
    try { g2(); } catch (e) { write(e); }
})();


(function () {
    try { g3(); } catch (e) { write(e); }

    var x = function g3() { write("first"); }
    try { g3(); } catch (e) { write(e); }

    var g3 = global;
    try { g3(); } catch (e) { write(e); }
})();

(function () {
    try { g4(); } catch (e) { write(e); }

    var g4 = global 
    try { g4(); } catch (e) { write(e); }

    var g4 = another
    try { g4(); } catch (e) { write(e); }
})();
