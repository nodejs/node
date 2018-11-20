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

description("ThrowTypeError is a singleton object");

function getter(object, name)
{
    Object.getOwnPropertyDescriptor(object, name).get;
}

function strictArgumentsFunction1()
{
    "use strict";
    return arguments;
}
var strictArguments1 = strictArgumentsFunction1();
var boundFunction1 = strictArgumentsFunction1.bind();
var functionCaller1 = getter(strictArgumentsFunction1, "caller");
var functionArguments1 = getter(strictArgumentsFunction1, "arguments");
var argumentsCaller1 = getter(strictArguments1, "caller");
var argumentsCallee1 = getter(strictArguments1, "callee");
var boundCaller1 = getter(boundFunction1, "caller");
var boundArguments1 = getter(boundFunction1, "arguments");

function strictArgumentsFunction2()
{
    "use strict";
    return arguments;
}
var strictArguments2 = strictArgumentsFunction2();
var boundFunction2 = strictArgumentsFunction2.bind();
var functionCaller2 = getter(strictArgumentsFunction2, "caller");
var functionArguments2 = getter(strictArgumentsFunction2, "arguments");
var argumentsCaller2 = getter(strictArguments2, "caller");
var argumentsCallee2 = getter(strictArguments2, "callee");
var boundCaller2 = getter(boundFunction2, "caller");
var boundArguments2 = getter(boundFunction2, "arguments");

shouldBeTrue('functionCaller1 === functionCaller2');

shouldBeTrue('functionCaller1 === functionArguments1');
shouldBeTrue('functionCaller1 === argumentsCaller1');
shouldBeTrue('functionCaller1 === argumentsCallee1');
shouldBeTrue('functionCaller1 === boundCaller1');
shouldBeTrue('functionCaller1 === boundArguments1');

shouldBeTrue('functionCaller2 === functionArguments2');
shouldBeTrue('functionCaller2 === argumentsCaller2');
shouldBeTrue('functionCaller2 === argumentsCallee2');
shouldBeTrue('functionCaller2 === boundCaller2');
shouldBeTrue('functionCaller2 === boundArguments2');

successfullyParsed = true;
