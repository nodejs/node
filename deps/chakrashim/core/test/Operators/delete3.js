//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

var r;

// Delete locals
(function Test1() {
    var p;
    var q = 10;

    write("p : " + p);
    r = delete p;
    write("delete p: " + r);
    write("p : " + p);

    write("q : " + q);
    r = delete q;
    write("delete q: " + r);
    write("q : " + q);
}) ();

test2_value = 10;
// Delete globals
(function Test2() {
    test2_local = 20;

    write("test2_value : " + test2_value);
    r = delete test2_value;
    write("delete test2_value: " + r);

    try {
        write("test2_value : " + test2_value);
        write("delete test2_value failed");
    } catch (e) {
        write("delete test2_value passed");
    }

    write("test2_local : " + test2_local);
    r = delete test2_local;
    write("delete test2_local: " + r);

    try {
        write("test2_local : " + test2_local);
        write("delete test2_local failed");
    } catch (e) {
        write("delete test2_local passed");
    }
}) ();

// Deleting parameters
(function Test3(x,y,z) {
    write("Test3 x : " + x + " y: " + y + " z: " + z);

    r = delete x;
    write("delete x: " + r);

    r = delete y;
    write("delete x: " + r);

    r = delete z;
    write("delete x: " + r);

    write("Test3 x : " + x + " y: " + y + " z: " + z);
 })(10, "hello");

// Delete arguments[i]
(function Test4(x,y,z) {
    write("Test4 x : " + x + " y: " + y + " z: " + z);
    write("Test4 arguments[0] : " + arguments[0] + " arguments[1]: " + arguments[1] + " arguments[2]: " + arguments[2]);

    r = delete arguments[0];
    write("delete arguments[0]: " + r);

    r = delete arguments[2];
    write("delete arguments[2]: " + r);

    write("Test4 x : " + x + " y: " + y + " z: " + z);
    write("Test4 arguments[0] : " + arguments[0] + " arguments[1]: " + arguments[1] + " arguments[2]: " + arguments[2]);
}) (10, "hello");

// Delete nested function
(function Test5() {

    function test5_func() { return 100;}

    r = delete test5_func;
    write("Test5: delete function " + r);
    write("test5_func() :" + test5_func());
}) ();

// Delete this
(function Test6() {
    try {
        r = delete this;
    } catch (e) {
        write("Exception delete this : " + e.message);
    }
    write("Test6: delete this: " + r);
}) ();

// Delete a global function
function test7_value() {}
(function Test7() {
    r = delete test7_value;
    write("Test7: delete test7_value: " + r);
}) ();

// Delete arguments
(function Test8() {
    r = delete arguments;
    write("Test8: delete arguments: " + r);
}) ();

// Delete exception
(function Test9() {
    try {
        throw 10;
    } catch(e) {
        r = delete e;
    }
    write("Test9: delete exception: " + r);
})();

// Delete exception
(function Test10() {
    try {
        throw new Error("some error");
    } catch(e) {
        r = delete e;
    }
    write("Test10: delete exception: " + r);
})();

// Delete variable declared using eval
(function Test11(){
    eval("var x = 10;");
    r = delete x;
    write("Test11: delete x: " + r);
}) ();

// Delete variable declared using eval, in eval
(function Test12(){
    eval("var x = 10;");
    r = eval("delete x");
    write("Test12: delete x: " + r);
}) ();

// Delete variable in eval
(function Test13(){
    var x;
    r = eval("delete x;");
    write("Test13: delete x: " + r);
}) ();

// Delete function in eval
(function Test14(){
    function f() { return 100;}
    r = eval("delete f;");
    write("Test14: delete f: " + r);
}) ();

// Delete function in eval, declared in eval
(function Test15(){
    eval("function f() { return 100;}");
    r = eval("delete f;");
    write("Test15: delete f: " + r);
}) ();

// Delete function, declared in eval
(function Test16(){
    eval("function f() { return 100;}");
    r = delete f;
    write("Test16: delete f: " + r);
}) ();

// Delete arguments in function declared and called in eval
(function Test17(){
    r = eval("function test17_value(){ return delete arguments; }; test17_value();");

    write("Test17: delete test17_value: " + r);
}) ();

// With cases

// Delete local variabale, not in with
(function Test18(){
    var test18_value = 10;
    var o = {};

    with(o){
        r = delete test18_value;
    }

    write("Test18: delete test18_value: " + r);
    write("test18_value : " + test18_value);
}) ();

// Delete varibale in with
(function Test19(){
    var test19_value = 10;
    var o = { test19_value : 20 };

    with(o){
        r = delete test19_value;
    }

    write("Test19: delete test19_value: " + r);
    write("test19_value : " + test19_value);
    write("o.test19_value : " + o.test19_value);
}) ();

// Delete local variabale, not in with inside eval
(function Test20(){
    var test20_value = 10;
    var o = {};

    with(o){
        r = eval("delete test20_value");
    }

    write("Test20: delete test20_value: " + r);
    write("test20_value : " + test20_value);
}) ();

// Delete variabale in with, inside eval
(function Test21(){
    var test21_value = 10;
    var o = { test21_value : 20 };

    with(o){
        r = eval("delete test21_value");
    }

    write("Test21: delete test21_value: " + r);
    write("test21_value : " + test21_value);
    write("o.test21_value : " + o.test21_value);
}) ();

// Delete variable in multi level with!!!
(function Test22(){
    var test22_value = 10;
    var o1 = { test22_value : 20 };
    var o2 = { test22_value : 30 };
    var o3 = { test22_value : 40 };
    var o4 = { test22_value : 50 };

    with(o1){
        with(o2){
            with(o3){
                with(o4){
                    r = delete test22_value;
                }
            }
        }
    }

    write("Test22: delete test22_value: " + r);
    write("test22_value : " + test22_value);
    write("o1.test22_value : " + o1.test22_value);
    write("o2.test22_value : " + o2.test22_value);
    write("o3.test22_value : " + o3.test22_value);
    write("o4.test22_value : " + o4.test22_value);
}) ();

(function Test23(){
    var test23_value = 10;
    var o1 = { test23_value : 20 };
    var o2 = { test23_value : 30 };
    var o3 = { test23_value : 40 };
    var o4 = { test23_value : 50 };

    with(o1){
        with(o2){
            with(o3){
                with(o4){
                    r = eval("delete test23_value");
                }
            }
        }
    }

    write("Test23: delete test23_value: " + r);
    write("test23_value : " + test23_value);
    write("o1.test23_value : " + o1.test23_value);
    write("o2.test23_value : " + o2.test23_value);
    write("o3.test23_value : " + o3.test23_value);
    write("o4.test23_value : " + o4.test23_value);
}) ();

var Test24_value = 1;
(function Test24(){
    var Test24_value = 10;
    var o = { Test24_value : 20 };

    with(o){
       r = delete Test24_value;
    }

    write("Test24: delete Test24_value: " + r);
    write("Test24_value : " + Test24_value);
    write("o.Test24_value : " + o.Test24_value);
}) ();

var Test25_value = 1;
(function Test25(){
    var o = { Test25_value : 20 };

    with(o){
       r = eval("delete Test25_value");
    }

    write("Test25: delete Test25_value: " + r);
    write("Test25_value : " + Test25_value);
    write("o.Test25_value : " + o.Test25_value);
}) ();

var Test26_value = 1;
(function Test26(){
    var o = new Object();

    with(o){
       r = delete Test26_value;
    }

    write("Test26: delete Test26_value: " + r);
    write("Test26_value : " + Test26_value);
    write("o.Test26_value : " + o.Test26_value);
}) ();

var Test27_value = 1;
(function Test27(){
    var o = new Object();

    with(o){
       r = delete Test27_value;
    }

    write("Test27: delete Test27_value: " + r);
    write("Test27_value : " + Test27_value);
    write("o.Test27_value : " + o.Test27_value);
}) ();

// Function Declaration. And eval with same name
function Func28()
{
    write("Func28.1 :" + typeof(Func28));

    eval("var Func28 = 1;");
    write("Func28.2 :" + typeof(Func28));

    r = delete Func28;
    write("Func28: delete Func28: " + r);
    write("Func28.3 :" + typeof(Func28));

    eval("var Func28 = 1;");
    write("Func28.4 :" + typeof(Func28));

    eval("r = delete Func28;");
    write("Func28: delete Func28: " + r);
    write("Func28.5 :" + typeof(Func28));
}
Func28();

(function Expr29()
{
    write("Expr29.1 :" + typeof(Expr29));

    eval("var Expr29 = 1;");
    write("Expr29.2 :" + typeof(Expr29));

    r = delete Expr29;
    write("Expr29: delete Expr29: " + r);
    write("Expr29.3 :" + typeof(Expr29));

    eval("var Expr29 = 1;");
    write("Expr29.4 :" + typeof(Expr29));

    eval("r = delete Expr29;");
    write("Expr29: delete Expr29: " + r);
    write("Expr29.5 :" + typeof(Expr29));
})()

function Func30()
{
    write("Func30.1 :" + typeof(Func30));
    eval("var Func30 = 1;");

    eval('write("Func30.2 :" + typeof(Func30));');
    eval('eval("var Func30 = {};");');
    eval('write("Func30.3 :" + typeof(Func30));');

    var str = '(function Func30_inner() {' +
              '   write("Func30.4 :" + typeof(Func30));' +
              '   r = delete Func30; ' +
              '   write("Func30: delete Func30: " + r);' +
              '   write("Func30.5 :" + typeof(Func30));' +
              '   r = delete Func30; ' +
              '   write("Func30: delete Func30: " + r);' +
              '   write("Func30.6 :" + typeof(Func30));' +
              '})();';

    eval(str);
    write("Func30.7 :" + typeof(Func30));
}
Func30();

(function Expr31()
{
    write("Expr31.1 :" + typeof(Expr31));
    eval("var Expr31 = 1;");

    eval('write("Expr31.2 :" + typeof(Expr31));');
    eval('eval("var Expr31 = {};");');
    eval('write("Expr31.3 :" + typeof(Expr31));');

    var str = '(function Expr31_inner() {' +
              '   write("Expr31.4 :" + typeof(Expr31));' +
              '   r = delete Expr31; ' +
              '   write("Expr31: delete Expr31: " + r);' +
              '   write("Expr31.5 :" + typeof(Expr31));' +
              '   r = delete Expr31; ' +
              '   write("Expr31: delete Expr31: " + r);' +
              '   write("Expr31.6 :" + typeof(Expr31));' +
              '})();';

    eval(str);
    write("Expr31.7 :" + typeof(Expr31));
})()
