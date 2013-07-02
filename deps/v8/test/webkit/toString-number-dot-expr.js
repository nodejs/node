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

description(
"This test checks that toString() round-trip on a function that has a expression of form 4..x does not lose its meaning."
+ " The expression accesses the property 'x' from number '4'."
);

// construct same test-case for different kinds of number literals. the switch is used to avoid
// individual returns getting optimized away (if the interpreter would do dead code elimination)

// testcase for number literal with decimal point, i.e '4.'
function f1(a) {
    switch(a) {
    case "member":
        return 4..x;
    case "arrayget":
        return 4.["x"];
    case "constr":
        return 4.();
    case "funccall":
        return 4..f();
    case "parenfunccall":
        return (4..x)();
    case "assignment":
        return 4..x = 33;
    case "assignment2":
        return 4..x >>>= 1;
    case "prefix":
        return ++4..x;
    case "postfix":
        return 4..x++;
   case "delete":
        delete 4..x;
        return 4..x;
    }

    return 0;
}

// '4. .'
function f2(a) {
    switch(a) {
    case "member":
        return 4. .x;
    case "arrayget":
        return 4. ["x"];
    case "constr":
        return 4.();
    case "funccall":
        return 4. .f();
    case "parenfunccall":
        return (4. .x)();
    case "assignment":
        return 4. .x = 33;
    case "assignment2":
        return 4. .x >>>= 1;
    case "prefix":
        return ++4. .x;
    case "postfix":
        return 4. .x++;
    case "delete":
        delete 4. .x;
        return 4. .x;
    }

    return 0;
}

// '4e20'
function f2(a) {
    switch(a) {
    case "member":
        return 4e20.x;
    case "arrayget":
        return 4e20["x"];
    case "constr":
        return 4e20();
    case "funccall":
        return 4e20.f();
    case "parenfunccall":
        return (4e20.x)();
    case "assignment":
        return 4e20.x = 33;
    case "assignment2":
        return 4e20.x >>>= 1;
    case "prefix":
        return ++4e20.x;
    case "postfix":
        return 4e20.x++;
    case "delete":
        delete 4e20.x;
        return 4e20.x;
    }

    return 0;
}

// '4.1e-20'
function f3(a) {
    switch(a) {
    case "member":
        return 4.1e-20.x;
    case "arrayget":
        return 4.1e-20["x"];
    case "constr":
        return 4.1e-20();
    case "funccall":
        return 4.1e-20.f();
    case "parenfunccall":
        return (4.1e-20.x)();
    case "assignment":
        return 4.1e-20.x = 33;
    case "assignment2":
        return 4.1e-20.x >>>= 1;
    case "prefix":
        return ++4.1e-20.x;
    case "postfix":
        return 4.1e-20.x++;
   case "delete":
        delete 4.1e-20.x;
        return 4.1e-20.x;
    }

    return 0;
}

// '4'
function f4(a) {
    switch(a) {
    case "member":
        return 4 .x;
    case "arrayget":
        return 4["x"];
    case "constr":
        return 4();
    case "funccall":
        return 4 .f();
    case "parenfunccall":
        return (4 .x)();
    case "assignment":
        return 4 .x = 33;
    case "assignment2":
        return 4 .x >>>= 1;
    case "prefix":
        return ++4 .x;
    case "postfix":
        return 4 .x++;
    case "delete":
        delete 4 .x;
        return 4 .x;

    }

    return 0;
}

// '(4)'
function f5(a) {
    switch(a) {
    case "member":
        return (4).x;
    case "arrayget":
        return (4)["x"];
    case "constr":
        return (4)();
    case "funccall":
        return (4).f();
    case "parenfunccall":
        return ((4).x)();
    case "assignment":
        return (4).x = 33;
    case "assignment2":
        return (4).x >>>= 1;
    case "prefix":
        return ++(4).x;
    case "postfix":
        return (4).x++;
    case "delete":
        delete (4).x;
        return (4).x;
    }

    return 0;
}
unevalf = function(x) { return '(' + x.toString() + ')'; };

function testToString(fn)
{
    shouldBe("unevalf(eval(unevalf(" + fn + ")))", "unevalf(" + fn + ")");
}

for(var i = 1; i < 6; ++i)
    testToString("f" + i);
