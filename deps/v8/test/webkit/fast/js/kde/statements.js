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
function testSwitch(v) {
  var result = "";
  switch (v) {
     case 0:
        result += 'a';
     case 1:
        result += 'b';
     case 1:
        result += 'c';
     case 2:
        result += 'd';
        break;
  }
  return result;
}

shouldBe("testSwitch(0)", "'abcd'");
shouldBe("testSwitch(1)", "'bcd'"); // IE agrees, NS disagrees
shouldBe("testSwitch(2)", "'d'");
shouldBe("testSwitch(false)", "''");

function testSwitch2(v) {
  var result = "";
  switch (v) {
     case 1:
        result += 'a';
        break;
     case 2:
        result += 'b';
        break;
     default:
        result += 'c';
     case 3:
        result += 'd';
        break;
  }
  return result;
}

shouldBe("testSwitch2(1)", "'a'");
shouldBe("testSwitch2(2)", "'b'");
shouldBe("testSwitch2(3)", "'d'");
shouldBe("testSwitch2(-1)", "'cd'");
shouldBe("testSwitch2('x')", "'cd'");

function testSwitch3(v) {
  var result = "";
  switch (v) {
     default:
        result += 'c';
     case 3:
        result += 'd';
     case 4:
        result += 'e';
        break;
  }
  return result;
};

shouldBe("testSwitch3(0)", "'cde'");
shouldBe("testSwitch3(3)", "'de'");
shouldBe("testSwitch3(4)", "'e'");

function testSwitch4(v) {
  var result = "";
  switch (v) {
     case 0:
        result += 'a';
        result += 'b';
        break;
  }
  return result;
};

shouldBe("testSwitch4(0)", "'ab'");
