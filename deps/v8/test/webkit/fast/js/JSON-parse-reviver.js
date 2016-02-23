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

description("Test behaviour of JSON reviver function.")
if (!Array.isArray)
    Array.isArray = function(o) { return o.constructor === Array; }

function arrayReviver(i,v) {
    if (i != "") {
        currentHolder = this;
        debug("");
        debug("Ensure the holder for our array is indeed an array");
        shouldBeTrue("Array.isArray(currentHolder)");
        shouldBe("currentHolder.length", "" + expectedLength);
        if (i > 0) {
            debug("");
            debug("Ensure that we always get the same holder");
            shouldBe("currentHolder", "lastHolder");
        }
        switch (Number(i)) {
        case 0:
            v = undefined;
            debug("");
            debug("Ensure that the holder already has all the properties present at the start of filtering");
            shouldBe("currentHolder[0]", '"a value"');
            shouldBe("currentHolder[1]", '"another value"');
            shouldBe("currentHolder[2]", '"and another value"');
            shouldBe("currentHolder[3]", '"to delete"');
            shouldBe("currentHolder[4]", '"extra value"');
            break;

        case 1:
            debug("");
            debug("Ensure that returning undefined has removed the property 0 from the holder during filtering.");
            shouldBeFalse("currentHolder.hasOwnProperty(0)");
            currentHolder[2] = "a replaced value";
            break;

        case 2:
            debug("");
            debug("Ensure that changing the value of a property is reflected while filtering.")
            shouldBe("currentHolder[2]", '"a replaced value"');
            value = v;
            debug("");
            debug("Ensure that the changed value is reflected in the arguments passed to the reviver");
            shouldBe("value", "currentHolder[2]");
            delete this[3];
            break;

        case 3:
            debug("");
            debug("Ensure that we visited a value that we have deleted, and that deletion is reflected while filtering.");
            shouldBeFalse("currentHolder.hasOwnProperty(3)");
            value = v;
            debug("");
            debug("Ensure that when visiting a deleted property value is undefined");
            shouldBeUndefined("value");
            v = "undelete the property";
            expectedLength = this.length = 3;
            break;

        case 4:
            if (this.length != 3) {
                testFailed("Did not call reviver for deleted property");
                expectedLength = this.length = 3;
                break;
            }

        case 5:
            testPassed("Ensured that property was visited despite Array length being reduced.");
            value = v;
            shouldBeUndefined("value");
            this[10] = "fail";
            break;

        default:
            testFailed("Visited unexpected property " + i + " with value " + v);
        }
    }
    lastHolder = this;
    return v;
}
expectedLength = 5;
var result = JSON.parse('["a value", "another value", "and another value", "to delete", "extra value"]', arrayReviver);
debug("");
debug("Ensure that we created the root holder as specified in ES5");
shouldBeTrue("'' in lastHolder");
shouldBe("result", "lastHolder['']");
debug("");
debug("Ensure that a deleted value is revived if the reviver function returns a value");
shouldBeTrue("result.hasOwnProperty(3)");

function objectReviver(i,v) {
    if (i != "") {
        currentHolder = this;
        shouldBeTrue("currentHolder != globalObject");
        if (seen) {
            debug("");
            debug("Ensure that we get the same holder object for each property");
            shouldBe("currentHolder", "lastHolder");
        }
        seen = true;
        switch (i) {
        case "a property":
            v = undefined;
            debug("");
            debug("Ensure that the holder already has all the properties present at the start of filtering");
            shouldBe("currentHolder['a property']", '"a value"');
            shouldBe("currentHolder['another property']", '"another value"');
            shouldBe("currentHolder['and another property']", '"and another value"');
            shouldBe("currentHolder['to delete']", '"will be deleted"');
            break;

        case "another property":
            debug("");
            debug("Ensure that returning undefined has correctly removed the property 'a property' from the holder object");
            shouldBeFalse("currentHolder.hasOwnProperty('a property')");
            currentHolder['and another property'] = "a replaced value";
            break;

        case "and another property":
            debug("Ensure that changing the value of a property is reflected while filtering.");
            shouldBe("currentHolder['and another property']", '"a replaced value"');
            value = v;
            debug("");
            debug("Ensure that the changed value is reflected in the arguments passed to the reviver");
            shouldBe("value", '"a replaced value"');
            delete this["to delete"];
            break;

        case "to delete":
            debug("");
            debug("Ensure that we visited a value that we have deleted, and that deletion is reflected while filtering.");
            shouldBeFalse("currentHolder.hasOwnProperty('to delete')");
            value = v;
            debug("");
            debug("Ensure that when visiting a deleted property value is undefined");
            shouldBeUndefined("value");
            v = "undelete the property";
            this["new property"] = "fail";
            break;
        default:
            testFailed("Visited unexpected property " + i + " with value " + v);
        }
    }
    lastHolder = this;
    return v;
}

debug("");
debug("Test behaviour of revivor used in conjunction with an object");
var seen = false;
var globalObject = this;
var result = JSON.parse('{"a property" : "a value", "another property" : "another value", "and another property" : "and another value", "to delete" : "will be deleted"}', objectReviver);
debug("");
debug("Ensure that we created the root holder as specified in ES5");
shouldBeTrue("lastHolder.hasOwnProperty('')");
shouldBeFalse("result.hasOwnProperty('a property')");
shouldBeTrue("result.hasOwnProperty('to delete')");
shouldBe("result", "lastHolder['']");

debug("");
debug("Test behaviour of revivor that introduces a cycle");
function reviveAddsCycle(i, v) {
    if (i == 0)
        this[1] = this;
    return v;
}

shouldThrow('JSON.parse("[0,1]", reviveAddsCycle)');

debug("");
debug("Test behaviour of revivor that introduces a new array classed object (the result of a regex)");
var createdBadness = false;
function reviveIntroducesNewArrayLikeObject(i, v) {
    if (i == 0 && !createdBadness) {
        this[1] = /(a)+/.exec("a");
        createdBadness = true;
    }
    return v;
}

shouldBe('JSON.stringify(JSON.parse("[0,1]", reviveIntroducesNewArrayLikeObject))', '\'[0,["a","a"]]\'');
