//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function TestMulSmall(){
    var i, x, y;
    var A = new Array(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);

    // with OF check
    for (i = 0; i < A.length; i++){
        x = (A[i] * -3);
        WScript.Echo("TestMul(OF): " + A[i] + "*-3 = " + x);

        y = x;
        x = (x * -3);
        WScript.Echo("TestMul(OF): " + y + "*-3 = " + x);

        x = (A[i] * -2);
        WScript.Echo("TestMul(OF): " + A[i] + "*-2 = " + x);

        y = x;
        x = (x * -2);
        WScript.Echo("TestMul(OF): " + y + "*-2 = " + x);

        x = (A[i] * -1);
        WScript.Echo("TestMul(OF): " + A[i] + "*-1 = " + x);

        y = x;
        x = (x * -1);
        WScript.Echo("TestMul(OF): " + y + "*-1 = " + x);

        x = (A[i] * 0);
        WScript.Echo("TestMul(OF): " + A[i] + "*0 = " + x);

        y = x;
        x = (x * 0);
        WScript.Echo("TestMul(OF): " + y + "*0 = " + x);

        x = (A[i] * 1);
        WScript.Echo("TestMul(OF): " + A[i] + "*1 = " + x);

        y = x;
        x = (x * 1);
        WScript.Echo("TestMul(OF): " + y + "*1 = " + x);

        x = (A[i] * 2);
        WScript.Echo("TestMul(OF): " + A[i] + "*2 = " + x);

        y = x;
        x = (x * 2);
        WScript.Echo("TestMul(OF): " + y+ "*2 = " + x);

        x = (A[i] * 3);
        WScript.Echo("TestMul(OF): " + A[i] + "*3 = " + x);

        y = x;
        x = (x * 3);
        WScript.Echo("TestMul(OF): " + y+ "*3= " + x);
    }

    // without OF check
    for (i = 1; i < 10; i++){
        x = (i * -3);
        WScript.Echo("TestMul(NOF): " + i + "*-3 = " + x);

        y = x;
        x = (x * -3);
        WScript.Echo("TestMul(NOF): " + y + "*-3 = " + x);

        x = (i * -2);
        WScript.Echo("TestMul(NOF): " + i + "*-2 = " + x);

        y = x;
        x = (x * -2);
        WScript.Echo("TestMul(NOF): " + y + "*-2 = " + x);

        x = (i * -1);
        WScript.Echo("TestMul(NOF): " + i + "*-1 = " + x);

        y = x;
        x = (x * -1);
        WScript.Echo("TestMul(NOF): " + y + "*-1 = " + x);

        x = (i * 0);
        WScript.Echo("TestMul(NOF): " + i + "*0 = " + x);

        y = x;
        x = (x * -0);
        WScript.Echo("TestMul(NOF): " + y + "*-0 = " + x);

        x = (i * 1);
        WScript.Echo("TestMul(NOF): " + i + "*1 = " + x);

        y = x;
        x = (x * 1);
        WScript.Echo("TestMul(NOF): " + y + "*1 = " + x);

        x = (i * 2);
        WScript.Echo("TestMul(NOF): " + i + "*2 = " + x);

        y = x;
        x = (x * 2);
        WScript.Echo("TestMul(NOF): " + y + "*2 = " + x);

        x = (i * 3);
        WScript.Echo("TestMul(NOF): " + i + "*3 = " + x);

        y = x;
        x = (x * 3);
        WScript.Echo("TestMul(NOF): " + y + "*3 = " + x);
    }
}

function TestMulLarge(){
    var X = new Array();
    var Y = new Array();
    var y;
    var i, j;
    // All without OF check
    // -(2^i)
    for (i = 1; i < 10; i++){
        X[2] = i * -4;
        X[3] = i * -8;
        X[4] = i * -16;
        X[5] = i * -32;
        X[6] = i * -64;
        X[7] = i * -128;
        X[8] = i * -256;
        X[9] = i * -512;
        X[10] = i * -1024;
        X[11] = i * -2048;
        X[12] = i * -4096;
        X[13] = i * -8192;
        X[14] = i * -16384;

        for (j = 2; j < X.length; j++)
            WScript.Echo("TestMul(NOF): " + i + "*-" + Math.pow(2, j) + " = " + X[j]);
    }

    // 2 ^ i
    for (i = 1; i < 10; i++){
        X[2] = i * 4;
        X[3] = i * 8;
        X[4] = i * 16;
        X[5] = i * 32;
        X[6] = i * 64;
        X[7] = i * 128;
        X[8] = i * 256;
        X[9] = i * 512;
        X[10] = i * 1024;
        X[11] = i * 2048;
        X[12] = i * 4096;
        X[13] = i * 8192;
        X[14] = i * 16384;

        for (j = 2; j < X.length; j++)
            WScript.Echo("TestMul(NOF): " + i + "*" + Math.pow(2, j) + " = " + X[j]);
    }

    // 2 ^ i + 1
    for (i = 1; i < 10; i++){
        X[2] = i * 5;
        X[3] = i * 9;
        X[4] = i * 17;
        X[5] = i * 33;
        X[6] = i * 65;
        X[7] = i * 129;
        X[8] = i * 257;
        X[9] = i * 513;
        X[10] = i * 1025;
        X[11] = i * 2049;
        X[12] = i * 4097;
        X[13] = i * 8193;
        X[14] = i * 16385;

        for (j = 2; j < X.length; j++)
            WScript.Echo("TestMul(NOF): " + i + "*" + (Math.pow(2, j) + 1) + " = " + X[j]);
    }

    // 2 ^ i - 1
    for (i = 1; i < 10; i++){
        X[2] = i * 3;
        X[3] = i * 7;
        X[4] = i * 15;
        X[5] = i * 31;
        X[6] = i * 63;
        X[7] = i * 127;
        X[8] = i * 255;
        X[9] = i * 511;
        X[10] = i * 1023;
        X[11] = i * 2047;
        X[12] = i * 4095;
        X[13] = i * 8191;
        X[14] = i * 16383;

        for (j = 2; j < X.length; j++)
            WScript.Echo("TestMul(NOF): " + i + "*" + (Math.pow(2, j) - 1)+ " = " + X[j]);
    }
}

function TestRem(){
    var A = new Array(10243, -2238, 324, -153, 449, -1042, 999, -4408, 1022, -112);
    var X = new Array();
    var i, j;

    for (i = 0; i < A.length; i++){
        X[2] = A[i] % 4
        X[3] = A[i] % 8
        X[4] = A[i] % 16
        X[5] = A[i] % 32
        X[6] = A[i] % 64
        X[7] = A[i] % 128
        X[8] = A[i] % 256
        X[9] = A[i] % 512
        X[10] = A[i] % 1024
        X[11] = A[i] % 2048
        X[12] = A[i] % 4096
        X[13] = A[i] % 8192
        X[14] = A[i] % 16384

        for (j = 2; j < X.length; j++)
            WScript.Echo("TestRem: " + A[i] + "%" + Math.pow(2, j) +" = " + X[j]);
    }
}

function test0(){
  var e = 1;
  e =359356164.1;
  e >>=2;
  e *=3;
  WScript.Echo("e = " + (e|0));

  e =359356164.1;
  e >>=2;
  e *=-3;
  WScript.Echo("e = " + (e|0));

  e =359356164.1;
  e >>=4;
  e *=5;
  WScript.Echo("e = " + (e|0));

  e =359356164.1;
  e >>=4;
  e *=-5;
  WScript.Echo("e = " + (e|0));

  e =359356164.1;
  e >>=8;
  e *=7;
  WScript.Echo("e = " + (e|0));

  e =359356164.1;
  e >>=8;
  e *=-7;
  WScript.Echo("e = " + (e|0));

  e =359356164.1;
  e >>=10;
  e *=17;
  WScript.Echo("e = " + (e|0));

  e =359356164.1;
  e >>=10;
  e *=-17;
  WScript.Echo("e = " + (e|0));
};

TestMulSmall();
TestMulSmall();
WScript.Echo("TestMulSmall done");

TestMulLarge();
TestMulLarge();
WScript.Echo("TestMulLarge done");

TestRem();
TestRem();
WScript.Echo("TestRem done");

test0()
test0();
WScript.Echo("Bug 326533 Test done");
