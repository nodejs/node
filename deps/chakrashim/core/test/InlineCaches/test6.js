//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

/*
test setpropertyscoped, setter in the lookup chain
*/
var a = function(){};
Object.defineProperty(a, "foo", {set: function() {WScript.Echo('in set');}, get: function() {return 5;}});
a.foo = 5;
a.foo = 5;
with (a)
{
eval("foo = 10");
eval("foo = 10");
}

// Make sure we can delete the getter *inside* the getter and avoid calling it 
var obj = {}; 
Object.defineProperty(obj, "test", { get: function() { delete obj.test; obj.test = 1; return 0; }, configurable: true }); 
WScript.Echo(obj.test + ''); // 0 
WScript.Echo(obj.test + '');

// Getter reentry, ensure we don't assert (WIN8: 388926)
(function(){
    var g = 0;

    var o = {};
    Object.defineProperty(o, "x", {
        get : function() {
            if (g == 0) {
                g = 1;
                return func();
            } else {
                return g;
            }
        }
    });

    function func()
    {
        return o.x;
    }

    WScript.Echo(func());
    WScript.Echo(func());
})();

// Change into data property inside getter, ensure we don't have invalid inline cache
(function(){
    var g = 0;

    var o = {};
    Object.defineProperty(o, "x", {
        get : function() {
            if (g == 0) {
                g = 1;
                Object.defineProperty(o, "x", {value: 5, writable: true});
                return func();
            } else {
                return g;
            }
        },
        configurable:true
    });

    function func()
    {
        return o.x;
    }

    WScript.Echo(func());
    WScript.Echo(func());
})();
