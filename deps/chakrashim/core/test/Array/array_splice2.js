//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(args)
{
  WScript.Echo(args);
}

var a;
var b;

write("Scenario 1");
a = [];
a.length = 20;
b = a.splice(0,1,10);
write(a);
write(b);

write("Scenario 2");
a = [];
a.length = 20;
b = a.splice(0,0,10);
write(a);
write(b);

write("Scenario 3");
a = [];
a.length = 20;
b = a.splice(0,10);
write(a);
write(b);

write("Scenario 4");
a = [];
a.length = 20;
b = a.splice(0,1,1);
write(a);
write(b);

write("Scenario 5");
a = [];
a.length = 20;
b = a.splice(10,1,1);
write(a);
write(b);


write("Scenario 6");
a = [];
b = a.splice(0,1,1);
write(a);
write(b);


write("Scenario 7");
a = [];
a[10] = 10;
a[15] = 20;
b = a.splice(10,1);
write(a);
write(b);


write("Scenario 8");
a = [];
a[10] = 10;
a[15] = 20;
b = a.splice(10,5);
write(a);
write(b);


write("Scenario 9");
a = [];
a[10] = 10;
a[15] = 20;
b = a.splice(10,5,20);
write(a);
write(b);

write("Scenario 10");
a = [];
a[10] = 10;
a[15] = 20;
b = a.splice(10,5,20);
write(a);
write(b);

write("Scenario 11");
a = [];
a[10] = 10;
a[15] = 20;
b = a.splice(10,10,20);
write(a);
write(b);

write("Scenario 12");
a = [];
a[10] = 10;
a[15] = 20;
b = a.splice(10,1,20,30,40);
write(a);
write(b);

write("Scenario 13");
a = [];
a[10] = 10;
a[15] = 20;
b = a.splice(10,0,20,30,40);
write(a);
write(b);

write("Scenario 13");
a = [];
a[10] = 10;
a[15] = 20;
b = a.splice(10,6,20,30,40,50,60,70);
write(a);
write(b);

write("Scenario 14");
a = [];
a[10] = 10;
a[15] = 20;
b = a.splice(10,6,20,30,40,50,60,70);
write(a);
write(b);

write("Scenario 15");
a = [];
a[10] = 10;
a[15] = 20;
b = a.splice(10,10,20,30,40,50,60,70);
write(a);
write(b);

write("Scenario 15");
a = [];
a[10] = 10;
a[11] = 11;
a[15] = 20;
a[16] = 21;
b = a.splice(10,10,20,30,40,50,60,70);
write(a);
write(b);

write("Scenario 16");
a = [];
a[40] = 123; // creates a 2nd segment
b = a.splice(30, 11); // splice in between the 2 segments
write(a);
write(b);


//------ overflow tests ---------
function echo(v) {
    WScript.Echo(v);
}

function guarded_call(func) {
    try {
        func();
    } catch (e) {
        echo(e.name + " : " + e.message);
    }
}

function dump_array(arr) {
    echo("length: " + arr.length);
    for (var p in arr) {
        if (+p == p) {
            echo("  " + p + ": " + arr[p]);
        }
    }
}

echo("--- splice overflow 1");
var a = [];
guarded_call(function () {
    a[4294967290] = 100;
    a.splice(4294967294, 0, 200, 201, 202, 203, 204);
});
dump_array(a);

echo("--- splice overflow 2");
var a = [];
guarded_call(function () {
    var base = 4294967290;
    for (var i = 0; i < 10; i++) {
        a[base + i] = 100 + i;
    }
    a.splice(4294967290, 0, 200, 201, 202, 203, 204, 205, 206);
});
dump_array(a);

echo("--- splice overflow 3");
var a = [];
guarded_call(function () {
    var base = 4294967290;
    for (var i = 0; i < 10; i++) {
        a[base + i] = 100 + i;
    }
    delete a[base + 3];
    a.splice(4294967290, 0, 200, 201, 202, 203, 204, 205, 206);
});
dump_array(a);

echo("--- splice overflow 3");
var a = [];
guarded_call(function () {
    var base = 4294967290;
    for (var i = 0; i < 10; i++) {
        a[base + i] = 100 + i;
    }
    delete a[base + 3];
    a.splice(4294967290, 2);
});
dump_array(a);


echo("--- splice object overflow");
Object.prototype.splice = Array.prototype.splice;
var obj = new Object();
obj.length = 4294967295;
obj[4294967294] = "Eze";
var arr = obj.splice(4294967293, 4294967295, 1, 2, 3);
echo(obj.length);

echo("--- splice object delete");
Object.prototype.splice = Array.prototype.splice;
var obj = new Object();
for (var i = 0; i < 10; i++) {
    obj[i] = 100 + i;
}
obj.length = 10;
delete obj[4];
dump_array(obj);
obj.splice(0, 0, 200, 201);
dump_array(obj);