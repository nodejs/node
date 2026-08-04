// Copyright 2010 the V8 project authors. All rights reserved.
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

Object.extend = function (dest, source) {
  for (property in source) dest[property] = source[property];
  return dest;
};

Object.extend ( Function.prototype,
{
  wrap : function (wrapper) {
    var method = this;
    var bmethod = (function(_method) {
      return function () {
        this.$$$parentMethodStore$$$ = this.$proceed;
        this.$proceed = function() { return _method.apply(this, arguments); };
      };
    })(method);
    var amethod = function () {
      this.$proceed = this.$$$parentMethodStore$$$;
      if (this.$proceed == undefined) delete this.$proceed;
      delete this.$$$parentMethodStore$$$;
    };
    var value = function() { bmethod.call(this); retval = wrapper.apply(this, arguments); amethod.call(this); return retval; };
    return value;
  }
});

String.prototype.cap = function() {
  return this.charAt(0).toUpperCase() + this.substring(1).toLowerCase();
};

String.prototype.cap = String.prototype.cap.wrap(
  function(each) {
    if (each && this.indexOf(" ") != -1) {
      return this.split(" ").map(
        function (value) {
          return value.cap();
        }
      ).join(" ");
    } else {
      return this.$proceed();
  }
});

Object.extend( Array.prototype,
{
  map : function(fun) {
    if (typeof fun != "function") throw new TypeError();
    var len = this.length;
    var res = new Array(len);
    var thisp = arguments[1];
    for (var i = 0; i < len; i++) { if (i in this) res[i] = fun.call(thisp, this[i], i, this); }
    return res;
  }
});
assertEquals("Test1 test1", "test1 test1".cap());
assertEquals("Test2 Test2", "test2 test2".cap(true));
