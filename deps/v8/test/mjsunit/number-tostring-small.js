// Copyright 2008 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// This file is a concatenation of the number-tostring and
// to-precision mjsunit tests where the mjsunit assert code has been
// removed.

// ----------------------------------------------------------------------
// toString
(NaN).toString();
(1/0).toString();
(-1/0).toString();
(0).toString();
(9).toString();
(90).toString();
(90.12).toString();
(0.1).toString();
(0.01).toString();
(0.0123).toString();
(111111111111111111111).toString();
(1111111111111111111111).toString();
(11111111111111111111111).toString();
(0.00001).toString();
(0.000001).toString();
(0.0000001).toString();
(0.00000012).toString();
(0.000000123).toString();
(0.00000001).toString();
(0.000000012).toString();
(0.0000000123).toString();

(-0).toString();
(-9).toString();
(-90).toString();
(-90.12).toString();
(-0.1).toString();
(-0.01).toString();
(-0.0123).toString();
(-111111111111111111111).toString();
(-1111111111111111111111).toString();
(-11111111111111111111111).toString();
(-0.00001).toString();
(-0.000001).toString();
(-0.0000001).toString();
(-0.00000012).toString();
(-0.000000123).toString();
(-0.00000001).toString();
(-0.000000012).toString();
(-0.0000000123).toString();

(NaN).toString(16);
(1/0).toString(16);
(-1/0).toString(16);
(0).toString(16);
(9).toString(16);
(90).toString(16);
(90.12).toString(16);
(0.1).toString(16);
(0.01).toString(16);
(0.0123).toString(16);
(111111111111111111111).toString(16);
(1111111111111111111111).toString(16);
(11111111111111111111111).toString(16);
(0.00001).toString(16);
(0.000001).toString(16);
(0.0000001).toString(16);
(0.00000012).toString(16);
(0.000000123).toString(16);
(0.00000001).toString(16);
(0.000000012).toString(16);
(0.0000000123).toString(16);

(-0).toString(16);
(-9).toString(16);
(-90).toString(16);
(-90.12).toString(16);
(-0.1).toString(16);
(-0.01).toString(16);
(-0.0123).toString(16);
(-111111111111111111111).toString(16);
(-1111111111111111111111).toString(16);
(-11111111111111111111111).toString(16);
(-0.00001).toString(16);
(-0.000001).toString(16);
(-0.0000001).toString(16);
(-0.00000012).toString(16);
(-0.000000123).toString(16);
(-0.00000001).toString(16);
(-0.000000012).toString(16);
(-0.0000000123).toString(16);

(2,32).toString();
(Math.pow(2,32)-1).toString(16);
(Math.pow(2,32)-1).toString(2);
(10000007).toString(36);
(0).toString(36);
(0).toString(16);
(0).toString(10);
(0).toString(8);
(0).toString(2);
(2,32).toString(2);
(Math.pow(2,32) + 1).toString(2);
(0x100000000000081).toString(16);
(-(-'0x1000000000000081')).toString(16);
(0x100000000000081).toString(2);
(-(Math.pow(2,32)-1)).toString(2);
(-10000007).toString(36);
(-Math.pow(2,32)).toString(2);
(-(Math.pow(2,32) + 1)).toString(2);
(-0x100000000000081).toString(16);
(-0x100000000000081).toString(2);
(1000).toString();
(0.00001).toString();
(1000000000000000128).toString();
(1000000000000000012800).toString();
(-1000000000000000012800).toString();
(0.0000001).toString();
(-0.0000001).toString();
(1000000000000000128000).toString();
(0.000001).toString();
(0.0000001).toString();
(8.5).toString(16);
(-8.5).toString(16);

// ----------------------------------------------------------------------
// toFixed
(NaN).toFixed(2);
(1/0).toFixed(2);
(-1/0).toFixed(2);

(1111111111111111111111).toFixed(8);
(0.1).toFixed(1);
(0.1).toFixed(2);
(0.1).toFixed(3);
(0.01).toFixed(2);
(0.01).toFixed(3);
(0.01).toFixed(4);
(0.001).toFixed(2);
(0.001).toFixed(3);
(0.001).toFixed(4);
(1).toFixed(4);
(1).toFixed(1);
(1).toFixed(0);
(12).toFixed(0);
(1.1).toFixed(0);
(12.1).toFixed(0);
(1.12).toFixed(0);
(12.12).toFixed(0);
(0.0000006).toFixed(7);
(0.00000006).toFixed(8);
(0.00000006).toFixed(9);
(0.00000006).toFixed(10);
(0).toFixed(0);
(0).toFixed(1);
(0).toFixed(2);

(-1111111111111111111111).toFixed(8);
(-0.1).toFixed(1);
(-0.1).toFixed(2);
(-0.1).toFixed(3);
(-0.01).toFixed(2);
(-0.01).toFixed(3);
(-0.01).toFixed(4);
(-0.001).toFixed(2);
(-0.001).toFixed(3);
(-0.001).toFixed(4);
(-1).toFixed(4);
(-1).toFixed(1);
(-1).toFixed(0);
(-1.1).toFixed(0);
(-12.1).toFixed(0);
(-1.12).toFixed(0);
(-12.12).toFixed(0);
(-0.0000006).toFixed(7);
(-0.00000006).toFixed(8);
(-0.00000006).toFixed(9);
(-0.00000006).toFixed(10);
(-0).toFixed(0);
(-0).toFixed(1);
(-0).toFixed(2);

(1000).toFixed();
(0.00001).toFixed();
(0.00001).toFixed(5);
(0.0000000000000000001).toFixed(20);
(0.00001).toFixed(17);
(1).toFixed(17);
(1000000000000000128).toFixed();
(100000000000000128).toFixed(1);
(10000000000000128).toFixed(2);
(10000000000000128).toFixed(20);
(0).toFixed();
((-42).toFixed(3));
(-1000000000000000128).toFixed();
(-0.0000000000000000001).toFixed(20);
(0.123123123123123).toFixed(20);
// Test that we round up even when the last digit generated is even.
// dtoa does not do this in its original form.
(0.5).toFixed(0);
(-0.5).toFixed(0);
(1.25).toFixed(1);
// This is bizare, but Spidermonkey and KJS behave the same.
(234.20405).toFixed(4);
(234.2040506).toFixed(4);

// ----------------------------------------------------------------------
// toExponential
(1).toExponential();
(11).toExponential();
(112).toExponential();
(1).toExponential(0);
(11).toExponential(0);
(112).toExponential(0);
(1).toExponential(1);
(11).toExponential(1);
(112).toExponential(1);
(1).toExponential(2);
(11).toExponential(2);
(112).toExponential(2);
(1).toExponential(3);
(11).toExponential(3);
(112).toExponential(3);
(0.1).toExponential();
(0.11).toExponential();
(0.112).toExponential();
(0.1).toExponential(0);
(0.11).toExponential(0);
(0.112).toExponential(0);
(0.1).toExponential(1);
(0.11).toExponential(1);
(0.112).toExponential(1);
(0.1).toExponential(2);
(0.11).toExponential(2);
(0.112).toExponential(2);
(0.1).toExponential(3);
(0.11).toExponential(3);
(0.112).toExponential(3);

(-1).toExponential();
(-11).toExponential();
(-112).toExponential();
(-1).toExponential(0);
(-11).toExponential(0);
(-112).toExponential(0);
(-1).toExponential(1);
(-11).toExponential(1);
(-112).toExponential(1);
(-1).toExponential(2);
(-11).toExponential(2);
(-112).toExponential(2);
(-1).toExponential(3);
(-11).toExponential(3);
(-112).toExponential(3);
(-0.1).toExponential();
(-0.11).toExponential();
(-0.112).toExponential();
(-0.1).toExponential(0);
(-0.11).toExponential(0);
(-0.112).toExponential(0);
(-0.1).toExponential(1);
(-0.11).toExponential(1);
(-0.112).toExponential(1);
(-0.1).toExponential(2);
(-0.11).toExponential(2);
(-0.112).toExponential(2);
(-0.1).toExponential(3);
(-0.11).toExponential(3);
(-0.112).toExponential(3);

(NaN).toExponential(2);
(Infinity).toExponential(2);
(-Infinity).toExponential(2);
(1).toExponential(0);
(0).toExponential();
(0).toExponential(2);
(11.2356).toExponential(0);
(11.2356).toExponential(4);
(0.000112356).toExponential(4);
(-0.000112356).toExponential(4);
(0.000112356).toExponential();
(-0.000112356).toExponential();

// ----------------------------------------------------------------------
// toPrecision
(NaN).toPrecision(1);
(Infinity).toPrecision(2);
(-Infinity).toPrecision(2);
(0.000555).toPrecision(15);
(0.000000555).toPrecision(15);
(-0.000000555).toPrecision(15);
(123456789).toPrecision(1);
(123456789).toPrecision(9);
(123456789).toPrecision(8);
(123456789).toPrecision(7);
(-123456789).toPrecision(7);
(-.0000000012345).toPrecision(2);
(-.000000012345).toPrecision(2);
(-.00000012345).toPrecision(2);
(-.0000012345).toPrecision(2);
(-.000012345).toPrecision(2);
(-.00012345).toPrecision(2);
(-.0012345).toPrecision(2);
(-.012345).toPrecision(2);
(-.12345).toPrecision(2);
(-1.2345).toPrecision(2);
(-12.345).toPrecision(2);
(-123.45).toPrecision(2);
(-1234.5).toPrecision(2);
(-12345).toPrecision(2);
(-12345.67).toPrecision(4);
Number(-12344.67).toPrecision(4);
// Test that we round up even when the last digit generated is even.
// dtoa does not do this in its original form.
(1.25).toPrecision(2);
(1.35).toPrecision(2);

// Test the exponential notation output.
(1.2345e+27).toPrecision(1);
(1.2345e+27).toPrecision(2);
(1.2345e+27).toPrecision(3);
(1.2345e+27).toPrecision(4);
(1.2345e+27).toPrecision(5);
(1.2345e+27).toPrecision(6);
(1.2345e+27).toPrecision(7);

(-1.2345e+27).toPrecision(1);
(-1.2345e+27).toPrecision(2);
(-1.2345e+27).toPrecision(3);
(-1.2345e+27).toPrecision(4);
(-1.2345e+27).toPrecision(5);
(-1.2345e+27).toPrecision(6);
(-1.2345e+27).toPrecision(7);


// Test the fixed notation output.
(7).toPrecision(1);
(7).toPrecision(2);
(7).toPrecision(3);

(-7).toPrecision(1);
(-7).toPrecision(2);
(-7).toPrecision(3);

(91).toPrecision(1);
(91).toPrecision(2);
(91).toPrecision(3);
(91).toPrecision(4);

(-91).toPrecision(1);
(-91).toPrecision(2);
(-91).toPrecision(3);
(-91).toPrecision(4);

(91.1234).toPrecision(1);
(91.1234).toPrecision(2);
(91.1234).toPrecision(3);
(91.1234).toPrecision(4);
(91.1234).toPrecision(5);
(91.1234).toPrecision(6);
(91.1234).toPrecision(7);
(91.1234).toPrecision(8);

(-91.1234).toPrecision(1);
(-91.1234).toPrecision(2);
(-91.1234).toPrecision(3);
(-91.1234).toPrecision(4);
(-91.1234).toPrecision(5);
(-91.1234).toPrecision(6);
(-91.1234).toPrecision(7);
(-91.1234).toPrecision(8);
