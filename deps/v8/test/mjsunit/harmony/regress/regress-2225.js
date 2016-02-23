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

var proxy_has_x = false;
var proxy = Proxy.create({ getPropertyDescriptor:function(key) {
  assertSame('x', key);
  if (proxy_has_x) {
    return { configurable:true, writable:false, value:19 };
  }
}});

// Test __lookupGetter__/__lookupSetter__ with proxy.
assertSame(undefined, Object.prototype.__lookupGetter__.call(proxy, 'foo'));
assertSame(undefined, Object.prototype.__lookupSetter__.call(proxy, 'bar'));
assertSame(undefined, Object.prototype.__lookupGetter__.call(proxy, '123'));
assertSame(undefined, Object.prototype.__lookupSetter__.call(proxy, '456'));

// Test __lookupGetter__/__lookupSetter__ with proxy in prototype chain.
var object = Object.create(proxy);
assertSame(undefined, Object.prototype.__lookupGetter__.call(object, 'foo'));
assertSame(undefined, Object.prototype.__lookupSetter__.call(object, 'bar'));
assertSame(undefined, Object.prototype.__lookupGetter__.call(object, '123'));
assertSame(undefined, Object.prototype.__lookupSetter__.call(object, '456'));

// Test inline constructors with proxy as prototype.
function f() { this.x = 23; }
f.prototype = proxy;
proxy_has_x = false;
assertSame(23, new f().x);
proxy_has_x = true;
assertSame(19, new f().x);

// Test inline constructors with proxy in prototype chain.
function g() { this.x = 42; }
g.prototype.__proto__ = proxy;
proxy_has_x = false;
assertSame(42, new g().x);
proxy_has_x = true;
assertSame(19, new g().x);
