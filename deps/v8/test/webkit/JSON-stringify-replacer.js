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

description("Test to ensure correct behaviour of replacer functions in JSON.stringify");

var object = {0:0, 1:1, 2:2, 3:undefined};
var array = [0, 1, 2, undefined];
function returnUndefined(){}
function returnObjectFor1(k, v) {
    if (k == "1")
        return {};
    return v;
}
function returnArrayFor1(k, v) {
    if (k == "1")
        return [];
    return v;
}
function returnUndefinedFor1(k, v) {
    if (k == "1")
        return undefined;
    return v;
}
function returnNullFor1(k, v) {
    if (k == "1")
        return null;
    return v;
}
function returnCycleObjectFor1(k, v) {
    if (k == "1")
        return object;
    return v;
}
function returnCycleArrayFor1(k, v) {
    if (k == "1")
        return array;
    return v;
}
function returnFunctionFor1(k, v) {
    if (k == "1")
        return function(){};
    return v;
}
function returnStringForUndefined(k, v) {
    if (v === undefined)
        return "undefined value";
    return v;
}

shouldBeUndefined("JSON.stringify(object, returnUndefined)");
shouldBeUndefined("JSON.stringify(array, returnUndefined)");

shouldBe("JSON.stringify(object, returnObjectFor1)", '\'{"0":0,"1":{},"2":2}\'');
shouldBe("JSON.stringify(array, returnObjectFor1)", '\'[0,{},2,null]\'');

shouldBe("JSON.stringify(object, returnArrayFor1)", '\'{"0":0,"1":[],"2":2}\'');
shouldBe("JSON.stringify(array, returnArrayFor1)", '\'[0,[],2,null]\'');

shouldBe("JSON.stringify(object, returnUndefinedFor1)", '\'{"0":0,"2":2}\'');
shouldBe("JSON.stringify(array, returnUndefinedFor1)", '\'[0,null,2,null]\'');

shouldBe("JSON.stringify(object, returnFunctionFor1)", '\'{"0":0,"2":2}\'');
shouldBe("JSON.stringify(array, returnFunctionFor1)", '\'[0,null,2,null]\'');

shouldBe("JSON.stringify(object, returnNullFor1)", '\'{"0":0,"1":null,"2":2}\'');
shouldBe("JSON.stringify(array, returnNullFor1)", '\'[0,null,2,null]\'');

shouldBe("JSON.stringify(object, returnStringForUndefined)", '\'{"0":0,"1":1,"2":2,"3":"undefined value"}\'');
shouldBe("JSON.stringify(array, returnStringForUndefined)", '\'[0,1,2,"undefined value"]\'');

shouldThrow("JSON.stringify(object, returnCycleObjectFor1)");
shouldThrow("JSON.stringify(array, returnCycleObjectFor1)");

shouldThrow("JSON.stringify(object, returnCycleArrayFor1)");
shouldThrow("JSON.stringify(array, returnCycleArrayFor1)");
