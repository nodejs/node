//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

function is_negative_zero(n)
{
    if(n == 0 && 1/n < 0)
        return true;
    else
        return false;
}

function itself(a) { return a; }

var x; var y;

// mul
x = 0*0;   write(" 0* 0 : " + x + " " + is_negative_zero(x));
x = -0*0;  write("-0* 0 : " + x + " " + is_negative_zero(x));
x = 0*-0;  write(" 0*-0 : " + x + " " + is_negative_zero(x));
x = -0*-0; write("-0*-0 : " + x + " " + is_negative_zero(x));

x = 5*5;   write(" 5* 5 : " + x + " " + is_negative_zero(x));
x = -5*5;  write("-5* 5 : " + x + " " + is_negative_zero(x));
x = 5*-5;  write(" 5*-5 : " + x + " " + is_negative_zero(x));
x = -5*-5; write("-5*-5 : " + x + " " + is_negative_zero(x));

x = 0*5;   write(" 0* 5 : " + x + " " + is_negative_zero(x));
x = -0*5;  write("-0* 5 : " + x + " " + is_negative_zero(x));
x = 0*-5;  write(" 0*-5 : " + x + " " + is_negative_zero(x));
x = -0*-5; write("-0*-5 : " + x + " " + is_negative_zero(x));

// Div
x = 0/0;   write(" 0/ 0 : " + x + " " + is_negative_zero(x));
x = -0/0;  write("-0/ 0 : " + x + " " + is_negative_zero(x));
x = 0/-0;  write(" 0/-0 : " + x + " " + is_negative_zero(x));
x = -0/-0; write("-0/-0 : " + x + " " + is_negative_zero(x));

x = 5/5;   write(" 5/ 5 : " + x + " " + is_negative_zero(x));
x = -5/5;  write("-5/ 5 : " + x + " " + is_negative_zero(x));
x = 5/-5;  write(" 5/-5 : " + x + " " + is_negative_zero(x));
x = -5/-5; write("-5/-5 : " + x + " " + is_negative_zero(x));

x = 0/5;   write(" 0/ 5 : " + x + " " + is_negative_zero(x));
x = -0/5;  write("-0/ 5 : " + x + " " + is_negative_zero(x));
x = 0/-5;  write(" 0/-5 : " + x + " " + is_negative_zero(x));
x = -0/-5; write("-0/-5 : " + x + " " + is_negative_zero(x));

// Mod
x = 0%0;   write(" 0% 0 : " + x + " " + is_negative_zero(x));
x = -0%0;  write("-0% 0 : " + x + " " + is_negative_zero(x));
x = 0%-0;  write(" 0%-0 : " + x + " " + is_negative_zero(x));
x = -0%-0; write("-0%-0 : " + x + " " + is_negative_zero(x));

x = 5%5;   write(" 5% 5 : " + x + " " + is_negative_zero(x));
x = -5%5;  write("-5% 5 : " + x + " " + is_negative_zero(x));
x = 5%-5;  write(" 5%-5 : " + x + " " + is_negative_zero(x));
x = -5%-5; write("-5%-5 : " + x + " " + is_negative_zero(x));

x = 0%5;   write(" 0% 5 : " + x + " " + is_negative_zero(x));
x = -0%5;  write("-0% 5 : " + x + " " + is_negative_zero(x));
x = 0%-5;  write(" 0%-5 : " + x + " " + is_negative_zero(x));
x = -0%-5; write("-0%-5 : " + x + " " + is_negative_zero(x));


y = 1 / (0 / -1); write(y + " " + (y === -Infinity));
y = 1 / (0 * -1); write(y + " " + (y === -Infinity));

y = 1 / (+0 / -1); write(y + " " + (y === Number.NEGATIVE_INFINITY));
y = 1 / (+0 * -1); write(y + " " + (y === Number.NEGATIVE_INFINITY));

y = 2 / (-5 % 5 ); write(y + " " + (y === Number.NEGATIVE_INFINITY));
y = -2/ (-5 % 5 ); write(y + " " + (y === Number.NEGATIVE_INFINITY));
y = 2 / (-5 %-5 ); write(y + " " + (y === Number.NEGATIVE_INFINITY));
y = -2/ (-5 %-5 ); write(y + " " + (y === Number.NEGATIVE_INFINITY));

    
function op_test() {
    var x = 0;
    var y = 0;

    if ( itself(1) )
    {
        x = 0;
        y = -1;
    }
    else
    {
        x = 5;
        y = 10;
    }
        
    var r;
    
    r = x * y; write(r + " " + is_negative_zero(r));
    r = x / y; write(r + " " + is_negative_zero(r));
    r = x % y; write(r + " " + is_negative_zero(r));
}

op_test();
