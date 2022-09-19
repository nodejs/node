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
shouldBe("Object.prototype.length","undefined");
shouldBe("Function.prototype.length","0");
shouldBe("Array.prototype.length","0");
shouldBe("String.prototype.length","0");
shouldBe("Boolean.prototype.length","undefined");
shouldBe("Number.prototype.length","undefined");
shouldBe("Date.prototype.length","undefined");
shouldBe("RegExp.prototype.length","undefined");
shouldBe("Error.prototype.length","undefined");

// check !ReadOnly
Array.prototype.length = 6;
shouldBe("Array.prototype.length","6");
// check ReadOnly
Function.prototype.length = 7;
shouldBe("Function.prototype.length","0");
String.prototype.length = 8;
shouldBe("String.prototype.length","0");

// check DontDelete
shouldBe("delete Array.prototype.length","false");
shouldBe("delete Function.prototype.length","true");
shouldBe("delete String.prototype.length","false");

// check DontEnum
var foundArrayPrototypeLength = false;
for (i in Array.prototype) { if (i == "length") foundArrayPrototypeLength = true; }
shouldBe("foundArrayPrototypeLength","false");

var foundFunctionPrototypeLength = false;
for (i in Function.prototype) { if (i == "length") foundFunctionPrototypeLength = true; }
shouldBe("foundFunctionPrototypeLength","false");

var foundStringPrototypeLength = false;
for (i in String.prototype) { if (i == "length") foundStringPrototypeLength = true; }
shouldBe("foundStringPrototypeLength","false");
