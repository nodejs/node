//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test1() {
    WScript.Echo('Test sticky and unicode getter on RegExp.prototype');
    WScript.Echo(RegExp.prototype.unicode);
    WScript.Echo(RegExp.prototype.sticky);
    
    function verifier(r)
    {
        WScript.Echo('Testing ' + r);
        r.unicode = true; // no-op
        r.sticky = true; // no-op
        WScript.Echo('unicode : ' + r.unicode);
        WScript.Echo('sticky : ' + r.sticky);
    }
    
    verifier(/.?\d+/);
    RegExp.prototype.unicode = true; // no-op
    verifier(/.?\d+/gu);
    verifier(/.?\d+/iy);
    RegExp.prototype.sticky = true; // no-op
    verifier(new RegExp("a*bc", "g"));
    verifier(new RegExp("a*bc", "u"));
    verifier(new RegExp("a*bc?","y"));
    verifier(new RegExp("a*b\d\s?","iuy"));    
}
test1();

function test2() {
    WScript.Echo('Test sticky bit with exec()');
    var pattern = /hello\d\s?/y;
    var text = "hello1 hello2 hello3";
    do {
        WScript.Echo(pattern.exec(text));
        WScript.Echo(pattern.lastIndex);
    } while(pattern.lastIndex > 0);
}
test2();

function test3() {
    WScript.Echo('Test sticky bit with test()');
    var pattern = /hello\d\s?/y;
    var text = "hello1 hello2 hello3";
    do {
        WScript.Echo(pattern.test(text));
        WScript.Echo(pattern.lastIndex);
    } while(pattern.lastIndex > 0);
}
test3();

function test4() {
    WScript.Echo('Test sticky bit with search()');
    var pattern = /hello\d\s?/y;
    var text = "hello1 hello2 hello3";
    do {
        WScript.Echo(text.search(pattern));
        WScript.Echo(pattern.lastIndex); // should break after 1st iteration
    } while(pattern.lastIndex > 0);
}
test4();

function test5() {
    WScript.Echo('Test sticky bit with replace()');
    var pattern = /hello\d\s?/y;
    var text = "hello1 hello2 hello3";
    // should replace 1st occurance because global is false and last index should be at 7.
    WScript.Echo(text.replace(pattern, "world "));
    WScript.Echo(pattern.lastIndex); 
}
test5();

function test6() {
    WScript.Echo('Test sticky bit with replace() with global flag.');
    var pattern = /hello\d\s?/g;
    var text = "hello1 hello2 hello3";
    // should replace all occurances because global is true and last index should be at 0.
    WScript.Echo(text.replace(pattern, "world "));
    WScript.Echo(pattern.lastIndex); 
}
test6();

function test7() {
    WScript.Echo('Test sticky bit with replace() with global/sticky flag.');
    var pattern = /hello\d\s?/gy;
    var text = "hello1 hello2 hello3";
    // should replace all occurances because global is true irrespective of sticky bit and last index should be at 0.
    WScript.Echo(text.replace(pattern, "world "));
    WScript.Echo(pattern.lastIndex); 
}
test7();

function test8() {
    WScript.Echo('Test sticky bit with match()');
    var pattern = /hello\d\s?/y;
    var text = "hello1 hello2 hello3";
    // should match 1st occurance because global is false and last index should be at 7.
    WScript.Echo(text.match(pattern));
    WScript.Echo(pattern.lastIndex);
}
test8();

function test9() {
    WScript.Echo('Test sticky bit with match() with global flag.');
    var pattern = /hello\d\s?/g;
    var text = "hello1 hello2 hello3";
    // should match all occurance because global is true and last index should be at 0.
    WScript.Echo(text.match(pattern));
    WScript.Echo(pattern.lastIndex);
}
test9();

function test10() {
    WScript.Echo('Test sticky bit with match() with global/sticky flag.');
    var pattern = /hello\d\s?/gy;
    var text = "hello1 hello2 hello3";
    // should match all occurance because global is true and last index should be at 0 irrespective of sticky bit flag.
    WScript.Echo(text.match(pattern));
    WScript.Echo(pattern.lastIndex);
}
test10();

function test11() {
    WScript.Echo('Test sticky bit with split()');
    var pattern = /\d/y;
    var text = "hello1 hello2 hello3";
    WScript.Echo(text.split(pattern));
    // sticky bit flag is ignored and lastIndex is set to 0.
    WScript.Echo(pattern.lastIndex);
}
test11();




// class MyRegEx extends RegExp {
    // toString() {return "a";}
// }

// var a = new MyRegEx();
// MyRegEx.prototype.unicode = true;
// WScript.Echo(MyRegEx.prototype.unicode);