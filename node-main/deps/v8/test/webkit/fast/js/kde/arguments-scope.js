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
// We can't use normal shouldBe here, since they'd eval in the wrong context...

function shouldBeOfType(msg, val, type) {
  if (typeof(val) != type)
    testFailed(msg + ": value has type " + typeof(val) + " , not:" + type);
  else
    testPassed(msg);
}

function test0() {
    var arguments;
    // var execution should not overwrite something that was
    // in scope beforehand -- e.g. the arguments thing
    shouldBeOfType("test0", arguments, 'object');
 }

function test1() {
    // No need to undef-initialize something in scope already!
    shouldBeOfType("test1", arguments, 'object');
    var arguments;
}

function test2(arguments) {
    // Formals OTOH can overwrite the args object
    shouldBeOfType("test2", arguments, 'number');
}


function test3() {
    // Ditto for functions..
    shouldBeOfType("test3", arguments, 'function');
    function arguments() {}
}

function test4() {
    // Here, the -declaration- part of the var below should have no
    // effect..
    shouldBeOfType('test4.(1)', arguments, 'object');
    var arguments = 4;
    // .. but the assignment shoud just happen
    shouldBeOfType('test4.(2)', arguments, 'number');
}


test0();
test1();
test2(42);
test3();
test4();
