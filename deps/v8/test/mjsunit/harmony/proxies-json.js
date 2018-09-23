// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Flags: --harmony-proxies

function testStringify(expected, object) {
  // Test fast case that bails out to slow case.
  assertEquals(expected, JSON.stringify(object));
  // Test slow case.
  assertEquals(expected, JSON.stringify(object, undefined, 0));
}

// Test serializing a proxy, function proxy and objects that contain them.
var handler1 = {
  get: function(target, name) {
    return name.toUpperCase();
  },
  enumerate: function(target) {
    return ['a', 'b', 'c'];
  },
  getOwnPropertyDescriptor: function(target, name) {
    return { enumerable: true };
  }
}

var proxy1 = Proxy.create(handler1);
testStringify('{"a":"A","b":"B","c":"C"}', proxy1);

var proxy_fun = Proxy.createFunction(handler1, function() { return 1; });
testStringify(undefined, proxy_fun);
testStringify('[1,null]', [1, proxy_fun]);

var parent1a = { b: proxy1 };
testStringify('{"b":{"a":"A","b":"B","c":"C"}}', parent1a);

var parent1b = { a: 123, b: proxy1, c: true };
testStringify('{"a":123,"b":{"a":"A","b":"B","c":"C"},"c":true}', parent1b);

var parent1c = [123, proxy1, true];
testStringify('[123,{"a":"A","b":"B","c":"C"},true]', parent1c);

// Proxy with side effect.
var handler2 = {
  get: function(target, name) {
    delete parent2.c;
    return name.toUpperCase();
  },
  enumerate: function(target) {
    return ['a', 'b', 'c'];
  },
  getOwnPropertyDescriptor: function(target, name) {
    return { enumerable: true };
  }
}

var proxy2 = Proxy.create(handler2);
var parent2 = { a: "delete", b: proxy2, c: "remove" };
var expected2 = '{"a":"delete","b":{"a":"A","b":"B","c":"C"}}';
assertEquals(expected2, JSON.stringify(parent2));
parent2.c = "remove";  // Revert side effect.
assertEquals(expected2, JSON.stringify(parent2, undefined, 0));

// Proxy with a get function that uses the first argument.
var handler3 = {
  get: function(target, name) {
    if (name == 'valueOf') return function() { return "proxy" };
    return name + "(" + target + ")";
  },
  enumerate: function(target) {
    return ['a', 'b', 'c'];
  },
  getOwnPropertyDescriptor: function(target, name) {
    return { enumerable: true };
  }
}

var proxy3 = Proxy.create(handler3);
var parent3 = { x: 123, y: proxy3 }
testStringify('{"x":123,"y":{"a":"a(proxy)","b":"b(proxy)","c":"c(proxy)"}}',
              parent3);

// Empty proxy.
var handler4 = {
  get: function(target, name) {
    return 0;
  },
  enumerate: function(target) {
    return [];
  },
  getOwnPropertyDescriptor: function(target, name) {
    return { enumerable: false };
  }
}

var proxy4 = Proxy.create(handler4);
testStringify('{}', proxy4);
testStringify('{"a":{}}', { a: proxy4 });

// Proxy that provides a toJSON function that uses this.
var handler5 = {
  get: function(target, name) {
    if (name == 'z') return 97000;
    return function(key) { return key.charCodeAt(0) + this.z; };
  },
  enumerate: function(target) {
    return ['toJSON', 'z'];
  },
  getOwnPropertyDescriptor: function(target, name) {
    return { enumerable: true };
  }
}

var proxy5 = Proxy.create(handler5);
testStringify('{"a":97097}', { a: proxy5 });

// Proxy that provides a toJSON function that returns undefined.
var handler6 = {
  get: function(target, name) {
    return function(key) { return undefined; };
  },
  enumerate: function(target) {
    return ['toJSON'];
  },
  getOwnPropertyDescriptor: function(target, name) {
    return { enumerable: true };
  }
}

var proxy6 = Proxy.create(handler6);
testStringify('[1,null,true]', [1, proxy6, true]);
testStringify('{"a":1,"c":true}', {a: 1, b: proxy6, c: true});

// Object containing a proxy that changes the parent's properties.
var handler7 = {
  get: function(target, name) {
    delete parent7.a;
    delete parent7.c;
    parent7.e = "5";
    return name.toUpperCase();
  },
  enumerate: function(target) {
    return ['a', 'b', 'c'];
  },
  getOwnPropertyDescriptor: function(target, name) {
    return { enumerable: true };
  }
}

var proxy7 = Proxy.create(handler7);
var parent7 = { a: "1", b: proxy7, c: "3", d: "4" };
assertEquals('{"a":"1","b":{"a":"A","b":"B","c":"C"},"d":"4"}',
             JSON.stringify(parent7));
assertEquals('{"b":{"a":"A","b":"B","c":"C"},"d":"4","e":"5"}',
             JSON.stringify(parent7));
