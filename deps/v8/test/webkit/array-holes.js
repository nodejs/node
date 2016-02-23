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

description("This tests that arrays have holes that you can see the prototype through, not just missing values.");

function isHole(array, index)
{
    if (index >= array.length)
        return "bad index: past length";
    // Check if we can see through the hole into another room.
    Array.prototype[index] = "room";
    var isHole = array[index] == "room";
    delete Array.prototype[index];
    return isHole;
}

function showHoles(array)
{
    var string = "[";
    for (i = 0; i < array.length; ++i) {
        if (i)
            string += ", ";
        if (isHole(array, i))
            string += "hole";
        else
            string += array[i];
    }
    string += "]";
    return string;
}

function returnTrue()
{
    return true;
}

var a;

function addToArray(arg)
{
    a.push(arg);
}

function addToArrayReturnFalse(arg)
{
    a.push(arg);
    return false;
}

function addToArrayReturnTrue(arg)
{
    a.push(arg);
    return true;
}

shouldBe("var a = []; a.length = 1; showHoles(a)", "'[hole]'");
shouldBe("var a = []; a[0] = undefined; showHoles(a)", "'[undefined]'");
shouldBe("var a = []; a[0] = undefined; delete a[0]; showHoles(a)", "'[hole]'");

shouldBe("showHoles([0, , 2])", "'[0, hole, 2]'");
shouldBe("showHoles([0, 1, ,])", "'[0, 1, hole]'");
shouldBe("showHoles([0, , 2].concat([3, , 5]))", "'[0, hole, 2, 3, hole, 5]'");
shouldBe("showHoles([0, , 2, 3].reverse())", "'[3, 2, hole, 0]'");
shouldBe("a = [0, , 2, 3]; a.shift(); showHoles(a)", "'[hole, 2, 3]'");
shouldBe("showHoles([0, , 2, 3].slice(0, 3))", "'[0, hole, 2]'");
shouldBe("showHoles([0, , 2, 3].sort())", "'[0, 2, 3, hole]'");
shouldBe("showHoles([0, undefined, 2, 3].sort())", "'[0, 2, 3, undefined]'");
shouldBe("a = [0, , 2, 3]; a.splice(2, 3, 5, 6); showHoles(a)", "'[0, hole, 5, 6]'");
shouldBe("a = [0, , 2, 3]; a.unshift(4); showHoles(a)", "'[4, 0, hole, 2, 3]'");
shouldBe("showHoles([0, , 2, 3].filter(returnTrue))", "'[0, 2, 3]'");
shouldBe("showHoles([0, undefined, 2, 3].filter(returnTrue))", "'[0, undefined, 2, 3]'");
shouldBe("showHoles([0, , 2, 3].map(returnTrue))", "'[true, hole, true, true]'");
shouldBe("showHoles([0, undefined, 2, 3].map(returnTrue))", "'[true, true, true, true]'");
shouldBe("a = []; [0, , 2, 3].every(addToArrayReturnTrue); showHoles(a)", "'[0, 2, 3]'");
shouldBe("a = []; [0, undefined, 2, 3].every(addToArrayReturnTrue); showHoles(a)", "'[0, undefined, 2, 3]'");
shouldBe("a = []; [0, , 2, 3].forEach(addToArray); showHoles(a)", "'[0, 2, 3]'");
shouldBe("a = []; [0, undefined, 2, 3].forEach(addToArray); showHoles(a)", "'[0, undefined, 2, 3]'");
shouldBe("a = []; [0, , 2, 3].some(addToArrayReturnFalse); showHoles(a)", "'[0, 2, 3]'");
shouldBe("a = []; [0, undefined, 2, 3].some(addToArrayReturnFalse); showHoles(a)", "'[0, undefined, 2, 3]'");
shouldBe("[0, , 2, 3].indexOf()", "-1");
shouldBe("[0, undefined, 2, 3].indexOf()", "1");
shouldBe("[0, , 2, 3].lastIndexOf()", "-1");
shouldBe("[0, undefined, 2, 3].lastIndexOf()", "1");

Object.prototype[1] = "peekaboo";

shouldBe("showHoles([0, , 2])", "'[0, hole, 2]'");
shouldBe("showHoles([0, 1, ,])", "'[0, 1, hole]'");
shouldBe("showHoles([0, , 2].concat([3, , 5]))", "'[0, peekaboo, 2, 3, peekaboo, 5]'");
shouldBe("showHoles([0, , 2, 3].reverse())", "'[3, 2, peekaboo, 0]'");
shouldBe("a = [0, , 2, 3]; a.shift(); showHoles(a)", "'[peekaboo, 2, 3]'");
shouldBe("showHoles([0, , 2, 3].slice(0, 3))", "'[0, peekaboo, 2]'");
shouldBe("showHoles([0, , 2, 3].sort())", "'[0, 2, 3, hole]'");
shouldBe("showHoles([0, undefined, 2, 3].sort())", "'[0, 2, 3, undefined]'");
shouldBe("a = [0, , 2, 3]; a.splice(2, 3, 5, 6); showHoles(a)", "'[0, hole, 5, 6]'");
shouldBe("a = [0, , 2, 3]; a.unshift(4); showHoles(a)", "'[4, 0, peekaboo, 2, 3]'");
shouldBe("showHoles([0, , 2, 3].filter(returnTrue))", "'[0, peekaboo, 2, 3]'");
shouldBe("showHoles([0, undefined, 2, 3].filter(returnTrue))", "'[0, undefined, 2, 3]'");
shouldBe("showHoles([0, , 2, 3].map(returnTrue))", "'[true, true, true, true]'");
shouldBe("showHoles([0, undefined, 2, 3].map(returnTrue))", "'[true, true, true, true]'");
shouldBe("a = []; [0, , 2, 3].every(addToArrayReturnTrue); showHoles(a)", "'[0, peekaboo, 2, 3]'");
shouldBe("a = []; [0, undefined, 2, 3].every(addToArrayReturnTrue); showHoles(a)", "'[0, undefined, 2, 3]'");
shouldBe("a = []; [0, , 2, 3].forEach(addToArray); showHoles(a)", "'[0, peekaboo, 2, 3]'");
shouldBe("a = []; [0, undefined, 2, 3].forEach(addToArray); showHoles(a)", "'[0, undefined, 2, 3]'");
shouldBe("a = []; [0, , 2, 3].some(addToArrayReturnFalse); showHoles(a)", "'[0, peekaboo, 2, 3]'");
shouldBe("a = []; [0, undefined, 2, 3].some(addToArrayReturnFalse); showHoles(a)", "'[0, undefined, 2, 3]'");
shouldBe("[0, , 2, 3].indexOf()", "-1");
shouldBe("[0, , 2, 3].indexOf('peekaboo')", "1");
shouldBe("[0, undefined, 2, 3].indexOf()", "1");
shouldBe("[0, , 2, 3].lastIndexOf()", "-1");
shouldBe("[0, , 2, 3].lastIndexOf('peekaboo')", "1");
shouldBe("[0, undefined, 2, 3].lastIndexOf()", "1");

delete Object.prototype[1];
