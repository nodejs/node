//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

(function Test1(x,y) {    
    write(x + " " + arguments[0]);
    write(y + " " + arguments[1]);

    x = 100;
    y = 200;

    write(x + " " + arguments[0]);
    write(y + " " + arguments[1]);
}) (10,20);

(function Test2(x,y) {
    write(x + " " + arguments[0]);
    write(y + " " + arguments[1]);

    arguments[0] = 100;
    arguments[1] = 200;

    write(x + " " + arguments[0]);
    write(y + " " + arguments[1]);
}) (10,20);

(function Test3(x,y) {    
    write(x + " " + arguments[0]);
    write(y + " " + arguments[1]);

    eval("arguments[0] = 100;");
    eval("y = 200;");

    write(x + " " + arguments[0]);
    write(y + " " + arguments[1]);
}) (10,20);