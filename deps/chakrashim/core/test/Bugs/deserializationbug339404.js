//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var lol = function() {
    var n = function() {}
    n(formatOutput());
};
function formatOutput() {
    var n = function() {}
    return n(/[-+]?[0-9]*\.?[0-9]+([eE][-+]?[0-9]+)?/g, function() {
    });
}
var __counter = 0;
__counter++;
var protoObj0 = {};
protoObj0.method1 = {
    init: function() {
        return function bar() {
        };
    }
}.init();
protoObj0.method1.prototype = {
    method1: function() {
        try {
            function v0() {
                var v1 = 1;
                Object.prototype.indexOf = String.prototype.indexOf;
                prop1 = {
                    toString: function() {
                        v1;
                    }
                }.indexOf();
                [].push(v1);
            }
            v0();
            [
              {},
              new protoObj0.method1()
            ][__counter].method1();
        } catch(ex) {
            lol();
        } finally {
        }
    }
};
for(var _strvar22 in (new Int8Array(1))) {
    var n = function() {}
    var m = function() {}
    n(m(new protoObj0.method1().method1()));
}
WScript.Echo("PASS");
