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

description("KDE JS Test");
// --------------------------------------------------------------------------------

var resolution = 251; // set to 1 for 100% coverage

function checkEncodeException(encodeFunctionName,c1,c2)
{
    if (c2 == undefined)
        shouldThrow(encodeFunctionName
            + "(String.fromCharCode(" + c1 + "))");
    else
        shouldThrow(encodeFunctionName
            + "(String.fromCharCode(" + c1 + ") + String.fromCharCode(" + c2 + "))");
}

function checkEncodeDecode(encodeFunctionName, decodeFunctionName, c1, c2)
{
    if (c2 == undefined)
        shouldBe(decodeFunctionName + "(" + encodeFunctionName
            + "(String.fromCharCode(" + c1 + ")))",
            "String.fromCharCode(" + c1 + ")");
    else
        shouldBe(decodeFunctionName + "(" + encodeFunctionName
            + "(String.fromCharCode(" + c1 + ") + String.fromCharCode(" + c2 + ")))",
            "String.fromCharCode(" + c1 + ") + String.fromCharCode(" + c2 + ")");
}

function checkWithFunctions(encodeFunction, decodeFunction)
{
    checkEncodeDecode(encodeFunction, decodeFunction, 0);
    checkEncodeDecode(encodeFunction, decodeFunction, 0xD7FF);

    checkEncodeDecode(encodeFunction, decodeFunction, 0xE000);
    checkEncodeDecode(encodeFunction, decodeFunction, 0xFFFD);
    checkEncodeDecode(encodeFunction, decodeFunction, 0xFFFE);
    checkEncodeDecode(encodeFunction, decodeFunction, 0xFFFF);

    checkEncodeException(encodeFunction, 0xDC00);
    checkEncodeException(encodeFunction, 0xDFFF);

    checkEncodeDecode(encodeFunction, decodeFunction, 0xD800, 0xDC00);
    checkEncodeDecode(encodeFunction, decodeFunction, 0xDBFF, 0xDC00);
    checkEncodeDecode(encodeFunction, decodeFunction, 0xD800, 0xDFFF);
    checkEncodeDecode(encodeFunction, decodeFunction, 0xDBFF, 0xDFFF);

    checkEncodeException(encodeFunction, 0xD800, 0);
    checkEncodeException(encodeFunction, 0xD800, 0xD7FF);
    checkEncodeException(encodeFunction, 0xD800, 0xD800);
    checkEncodeException(encodeFunction, 0xD800, 0xDBFF);
    checkEncodeException(encodeFunction, 0xD800, 0xE000);
    checkEncodeException(encodeFunction, 0xD800, 0xE000);
    checkEncodeException(encodeFunction, 0xD800, 0xFFFD);
    checkEncodeException(encodeFunction, 0xD800, 0xFFFE);
    checkEncodeException(encodeFunction, 0xD800, 0xFFFF);

    for (var charcode = 1; charcode < 0xD7FF; charcode += resolution)
        checkEncodeDecode(encodeFunction, decodeFunction, charcode);

    for (var charcode = 0xE001; charcode < 0xFFFD; charcode += resolution)
        checkEncodeDecode(encodeFunction, decodeFunction, charcode);

    for (var charcode = 0xDC01; charcode < 0xDFFF; charcode += resolution)
        checkEncodeException(encodeFunction, charcode);

    for (var charcode = 0xD801; charcode < 0xDBFF; charcode += resolution)
        checkEncodeDecode(encodeFunction, decodeFunction, charcode, 0xDC00);

    for (var charcode = 0xDC01; charcode < 0xDFFF; charcode += resolution)
        checkEncodeDecode(encodeFunction, decodeFunction, 0xD800, charcode);

    for (var charcode = 1; charcode < 0xDBFF; charcode += resolution)
        checkEncodeException(encodeFunction, 0xD800, charcode);

    for (var charcode = 0xE001; charcode < 0xFFFD; charcode += resolution)
        checkEncodeException(encodeFunction, 0xD800, charcode);
}

checkWithFunctions("encodeURI", "decodeURI");
checkWithFunctions("encodeURIComponent", "decodeURIComponent");
