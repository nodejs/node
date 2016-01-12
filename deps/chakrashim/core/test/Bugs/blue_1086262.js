//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

﻿var __counter = 0;

function test0() {
    var loopInvariant = 0;
    var GiantPrintArray = [];
    __counter++;

    function makeArrayLength() { }

    function leaf() { }
    var obj0 = {};
    var protoObj0 = {};
    var obj1 = {};
    var protoObj1 = {};
    var obj2 = {};
    var protoObj2 = {};
    var arrObj0 = {};
    var litObj0 = {};
    var litObj1 = {};
    var litObj2 = {};
    var func0 = function() { };
    var func1 = function() { };
    var func2 = function() { };
    var func4 = function() {
        return ++protoObj2.prop4;
    };
    obj0.method0 = func4;
    obj1.method1 = func4;
    arrObj0.method0 = obj1.method1;
    arrObj0.method1 = func1;
    Object.method0 = obj1;
    prototype = arrObj0;
    var ary = Array();
    var i8 = new Int8Array();
    var i16 = new Int16Array();
    var i32 = new Int32Array(256);
    var IntArr0 = [];
    var IntArr1 = new Array();
    var FloatArr0 = Array();
    var VarArr0 = Array(4294967297, 1127376511);
    ary[ary.length] = 7373454682063640000;
    6543166720345670000;
    Object.prototype.prop4 = 88;
    protoObj1 = Object(obj1);
    protoObj2 = Object.create(obj2);
    var aliasOfary = ary;
    423453669;
    prop1 = -524802969.9;
    this;
    this.prop4;
    this;
    obj0;
    6543166720345670000;
    obj0.prop2 = -86849592;
    obj0.prop3;
    obj0.prop4;
    obj0.prop5;
    protoObj0.prop0;
    6543166720345670000;
    protoObj0.prop2;
    protoObj0.prop3;
    972908182993094000;
    if(false) {
        function func12() {
            this.prop0 = new protoObj0.method0(Object.arrObj0, /([b7]|蒤bba|[b7])?/im, new obj0.method0(protoObj1.prop10, /([b7])/m, typeof protoObj2.length != 'number', obj0).prop6, litObj2).undefined < VarArr0[17];
        }
        var __loopvar1 = loopInvariant;
        var uniqobj9 = new func12();
    } else {
        if(new Error() instanceof (typeof func4 == 'function' ? func4 : Object) < obj1.method1(protoObj2, /(?=\s\b\w)$/im, obj0.undefined--, protoObj1.prop10)) {
            var __loopvar2 = loopInvariant;
            LABEL0: LABEL1: for(; ;) {
                if(__loopvar2 > loopInvariant + 9) {
                    break;
                }
                __loopvar2 += 3;
            }
        } else {
            var uniqobj10 = new func0();
            obj9 = new protoObj1.method0();
        }

        function func15(arg0) {
            this.prop0 = arg0;
        }
        var uniqobj11 = new func15(new obj0.method0(protoObj1, /(?!a蒤a郳)/g, arrObj0.method1(protoObj1.prop10, /(?!a蒤a郳)/g), obj2).prop6);
        var uniqobj12 = new func15(i32[new Error() instanceof (typeof func4 == 'function' ? func4 : Object) < obj1.method1(protoObj2, /(?=\s\b\w)$/im, obj0.prop1--, protoObj1.prop10) & 255]);
        var loopInvariant = loopInvariant + 6,
            __loopSecondaryVar1_0 = loopInvariant,
            __loopSecondaryVar1_1 = loopInvariant + 12;
        LABEL0: while(IntArr0[(new protoObj1.method1(obj1, /[b7]$/m, arrObj0[__loopSecondaryVar1_1 - 1], Object.arrObj0).prop6 << IntArr1.unshift(new Error() instanceof (typeof func4 == 'function' ? func4 : Object) ? typeof -51369130694821500 != 'undefined' : new obj0.method0(protoObj1, /(?!a蒤a郳)/g, arrObj0.method1(protoObj1.prop10, /(?!a蒤a郳)/g), obj2).prop6, typeof uniqobj12.prop0 != 'number') >= 0 ? new protoObj1.method1(obj1, /[b7]$/m, arrObj0[__loopSecondaryVar1_1 - 1], Object.prototype).prop6 << IntArr1.unshift(new Error() instanceof (typeof func4 == 'function' ? func4 : Object) ? typeof -51369130694821500 != 'undefined' : new obj0.method0(protoObj1, /(?!a蒤a郳)/g, arrObj0.method1(protoObj1.prop10, /(?!a蒤a郳)/g), obj2).prop6, typeof uniqobj12.prop0 != 'number') : 0) & 15]) {
            loopInvariant -= 2;
            if(loopInvariant === loopInvariant - 2) {
                break;
            }
            loopInvariant = 2;
            v4(v10);
        }
    }
    var uniqobj15 = [protoObj0, arrObj0, arrObj0];
    uniqobj15[__counter].method0();
    Object.prop4;
    WScript.Echo('subset_of_ary = ' + ary.slice());
}
try {
    test0();
} catch(ex) {
    WScript.Echo(ex.message);
}
try {
    test0();
} catch(ex) {
    WScript.Echo(ex.message);
}
try {
    test0();
} catch(ex) {
    WScript.Echo(ex.message);
}
