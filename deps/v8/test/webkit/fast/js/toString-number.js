// Copyright 2013 the V8 project authors. All rights reserved.
// Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1.  Redistributions of source code must retain the above copyright
//     notice, this list of conditions and the following disclaimer.
// 2.  Redistributions in binary form must reproduce the above copyright
//     notice, this list of conditions and the following disclaimer in the
//     documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
// ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

description("Test the conversion performed by the function Number.prototype.toString.");

    var numberIndex = 0;
    var base10StringIndex = 1;
    var base2StringIndex = 2;
    var base36StringIndex = 3;

    var validNumberData = [
    // Regular Integers:
    [0, '0', '0', '0'],
    [-1, '-1', '-1', '-1'],
    [1, '1', '1', '1'],
    [1984, '1984', '11111000000', '1j4'],
    [-1984, '-1984', '-11111000000', '-1j4'],
    // Limits:
    [2147483647, '2147483647', '1111111111111111111111111111111', 'zik0zj'], // INT_MAX.
    [-2147483648, '-2147483648', '-10000000000000000000000000000000', '-zik0zk'], // INT_MIN
    [9007199254740992, '9007199254740992', '100000000000000000000000000000000000000000000000000000', '2gosa7pa2gw'], // Max Integer in a double.
    [-9007199254740992, '-9007199254740992', '-100000000000000000000000000000000000000000000000000000', '-2gosa7pa2gw'], // Min Integer in a double.

    // Integers represented as double.
    [0.0, '0', '0', '0'],
    [-1.0, '-1', '-1', '-1'],
    [1.0, '1', '1', '1'],
    [1984.0, '1984', '11111000000', '1j4'],
    [-1984.0, '-1984', '-11111000000', '-1j4'],
    // Limits:
    [2147483647.0, '2147483647', '1111111111111111111111111111111', 'zik0zj'], // INT_MAX.
    [-2147483648.0, '-2147483648', '-10000000000000000000000000000000', '-zik0zk'], // INT_MIN
    [9007199254740992.0, '9007199254740992', '100000000000000000000000000000000000000000000000000000', '2gosa7pa2gw'], // Max Integer in a double.
    [-9007199254740992.0, '-9007199254740992', '-100000000000000000000000000000000000000000000000000000', '-2gosa7pa2gw'], // Min Integer in a double.

    // Double.
    [0.1, '0.1', '0.0001100110011001100110011001100110011001100110011001101', '0.3lllllllllm'],
    [-1.1, '-1.1', '-1.000110011001100110011001100110011001100110011001101', '-1.3llllllllm'],
    [1.1, '1.1', '1.000110011001100110011001100110011001100110011001101', '1.3llllllllm'],
    [1984.1, '1984.1', '11111000000.00011001100110011001100110011001100110011', '1j4.3lllllllc'],
    [-1984.1, '-1984.1', '-11111000000.00011001100110011001100110011001100110011', '-1j4.3lllllllc'],
    // Limits:
    [2147483647.1, '2147483647.1', '1111111111111111111111111111111.000110011001100110011', 'zik0zj.3lllg'],
    [-2147483648.1, '-2147483648.1', '-10000000000000000000000000000000.000110011001100110011', '-zik0zk.3lllg'],
    [9007199254740992.1, '9007199254740992', '100000000000000000000000000000000000000000000000000000', '2gosa7pa2gw'],
    [-9007199254740992.1, '-9007199254740992', '-100000000000000000000000000000000000000000000000000000', '-2gosa7pa2gw'],
    ];

    for (var i = 0; i < validNumberData.length; ++i) {
        number = validNumberData[i][numberIndex];

        // Base 10:
        stringBase10 = validNumberData[i][base10StringIndex];
        shouldBeEqualToString('Number(' + number + ').toString()', stringBase10);
        shouldBeEqualToString('Number.prototype.toString.call(' + number + ')', stringBase10);
        shouldBeEqualToString('Number.prototype.toString.call(new Number(' + number + '))', stringBase10);
        // Passing the string to number should also lead to valid conversion.
        shouldBeEqualToString('Number("' + number + '").toString()', stringBase10);
        // Passing the base explicitly.
        shouldBeEqualToString('Number(' + number + ').toString(10)', stringBase10);

        // Base 2:
        stringBase2 = validNumberData[i][base2StringIndex];
        shouldBeEqualToString('Number(' + number + ').toString(2)', stringBase2);
        shouldBeEqualToString('Number.prototype.toString.call(' + number + ', 2)', stringBase2);
        shouldBeEqualToString('Number.prototype.toString.call(new Number(' + number + '), 2)', stringBase2);

        // Base 36:
        stringBase36 = validNumberData[i][base36StringIndex];
        shouldBeEqualToString('Number(' + number + ').toString(36)', stringBase36);
        shouldBeEqualToString('Number.prototype.toString.call(' + number + ', 36)', stringBase36);
        shouldBeEqualToString('Number.prototype.toString.call(new Number(' + number + '), 36)', stringBase36);
    }
    successfullyParsed = true;
