//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

// No arguments
var f = new Function();
write(f());

// Just the body
var f0 = new Function("return 10;");
write(f0());


var f1 = new Function("a", "return a;");
write(f1());
write(f1(100));


var f2 = new Function("a", "b", "return a+b;");
write(f2());
write(f2(10));
write(f2(10,20));


// All of f3? should be the same
var f31 = new Function("a", "b", "c", "return a+b+c;");
var f32 = new Function("a,b,c", "return a+b+c;");
var f33 = new Function("a,b", "c", "return a+b+c;");

write(f31());
write(f32());
write(f33());

write(f31(10,20,30));
write(f32(10,20,30));
write(f33(10,20,30));


// Check the name binding
var x = "global";
function fNameBinding() {
    var x = "local";
    var y = new Function("return x;");
    
    write(y());
    
    return x + " " + y();
}

write(fNameBinding());

