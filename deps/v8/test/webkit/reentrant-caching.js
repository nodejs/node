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

if (this.description)
    description("Test caching with re-entrancy.");

function test1() {
    var objects = [{prop:1}, {get prop(){return 2}}];

    function g(o) {
        return o.prop;
    }

    for (var i = 0; i < 10000; i++) {
        var o = {
            get prop() {
                try {
                    g(objects[++j]);
                }catch(e){
                }
                return 1;
            }
        };
        o[i] = i;
        objects.push(o);
    }
    var j=0;
    g(objects[0]);
    g(objects[1]);
    g(objects[2]);
    g(objects[3]);
}


function test2() {
    var objects = [Object.create({prop:1}), Object.create({get prop(){return 2}})];

    function g(o) {
        return o.prop;
    }
    var proto = {
        get prop() {
            try {
                g(objects[++j]);
            }catch(e){
            }
            return 1;
        }
    };
    for (var i = 0; i < 10000; i++) {
        var o = Object.create(proto);
        o[i] = i;
        objects.push(o);
    }
    var j=0;
    g(objects[0]);
    g(objects[1]);
    g(objects[2]);
    g(objects[3]);
}


function test3() {
    var objects = [Object.create(Object.create({prop:1})), Object.create(Object.create({get prop(){return 2}}))];

    function g(o) {
        return o.prop;
    }
    var proto = {
        get prop() {
            try {
                g(objects[++j]);
            }catch(e){
            }
            return 1;
        }
    };
    for (var i = 0; i < 10000; i++) {
        var o = Object.create(Object.create(proto));
        o[i] = i;
        objects.push(o);
    }
    var j=0;
    g(objects[0]);
    g(objects[1]);
    g(objects[2]);
    g(objects[3]);
}

test1();
test2();
test3();
