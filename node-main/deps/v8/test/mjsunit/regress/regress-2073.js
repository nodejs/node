// Copyright 2013 the V8 project authors. All rights reserved.
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

// Running this test with --trace_gc will show heap size growth due to
// leaking objects via embedded maps in optimized code.

var counter = 0;

function nextid() {
  counter += 1;
  return counter;
}

function Scope() {
  this.id = nextid();
  this.parent = null;
  this.left = null;
  this.right = null;
  this.head = null;
  this.tail = null;
  this.counter = 0;
}

Scope.prototype = {
  new: function() {
    var Child,
        child;
    Child = function() {};
    Child.prototype = this;
    child = new Child();
    child.id = nextid();
    child.parent = this;
    child.left = this.last;
    child.right = null;
    child.head = null;
    child.tail = null;
    child.counter = 0;
    if (this.head) {
      this.tail.right = child;
      this.tail = child;
    } else {
      this.head = this.tail = child;
    }
    return child;
  },

  destroy: function() {
    if ($root == this) return;
    var parent = this.parent;
    if (parent.head == this) parent.head = this.right;
    if (parent.tail == this) parent.tail = this.left;
    if (this.left) this.left.right = this.right;
    if (this.right) this.right.left = this.left;
  }
};

function inc(scope) {
  scope.counter = scope.counter + 1;
}

var $root = new Scope();

n = 100000;
m = 10;

function doit() {
   var a = $root.new();
   var b = a.new();
   inc(b);
   if (i > m) $root.head.destroy();
}

for (var i = 0; i < n; i++) {
   doit();
}
