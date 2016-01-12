//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

var x = "g.x";
var y = "g.y";
var z = "g.z";

function f1(x,x) {
    write('f1      : ' + Array.prototype.join.call(arguments) + '. x:' + x + ' y:' + y + ' z:' + z);
    eval("write('f1(eval): ' + Array.prototype.join.call(arguments) + '. x:' + x + ' y:' + y + ' z:' + z)");
}

function f2(x,x,x,x,x) {
    write('f2      : ' + Array.prototype.join.call(arguments) + '. x:' + x + ' y:' + y + ' z:' + z);
    eval("write('f2(eval): ' + Array.prototype.join.call(arguments) + '. x:' + x + ' y:' + y + ' z:' + z)");
}

function f3(x,y,x) {
    write('f3      : ' + Array.prototype.join.call(arguments) + '. x:' + x + ' y:' + y + ' z:' + z);
    eval("write('f3(eval): ' + Array.prototype.join.call(arguments) + '. x:' + x + ' y:' + y + ' z:' + z)");
}

function f4(x,y,y,x,z,z) {
    write('f4      : ' + Array.prototype.join.call(arguments) + '. x:' + x + ' y:' + y + ' z:' + z);
    eval("write('f4(eval): ' + Array.prototype.join.call(arguments) + '. x:' + x + ' y:' + y + ' z:' + z)");
}


for (var i = 1; i < 5; i++) {
    var fnc = "f" + i + "(";
    var para = "";
    
    for (var j = 1; j < 8; j++) {
        eval(fnc + para + ");");
        if (j != 1) {
            para += ", ";
        }
        para += j;
    }
}


function w1(x, x) {
    with ({})
        write("w1 : " + arguments[0] + " " + arguments[1]);
}

w1(50, 60);