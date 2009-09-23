// Copyright 2009 the V8 project authors. All rights reserved.
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

var global = this;

function ES5Error(ut) {
  this.ut = ut;
}

ES5Error.prototype.toString = function () {
  return this.ut.res;
};

// The harness uses the IE specific .description property of exceptions but
// that's nothing we can't hack our way around.
Error.prototype.__defineGetter__('description', function () {
  return this.message;
});

function TestHarness() {
  sth.call(this, global);
  this._testResults = []
}

// Borrow sth's registerTest method.
TestHarness.prototype.registerTest = sth.prototype.registerTest;

// Drop the before/after stuff, just run the test.
TestHarness.prototype.startTesting = function () {
  sth.prototype.run.call(this);
  this.report();
};

TestHarness.prototype.report = function () {
  for (var i = 0; i < this._testResults.length; i++) {
    var ut = this._testResults[i];
    // We don't fail on preconditions.  Yet.
    if (ut.res == "Precondition failed")
      continue;
    if (ut.res != 'pass')
      throw new ES5Error(ut);
  }
};

TestHarness.prototype.startingTest = function (ut) {
  this.currentTest = ut;
  this._testResults.push(ut);
};

var ES5Harness = new TestHarness();
