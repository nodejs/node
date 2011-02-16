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

// Load source code files from <project root>/tools.
// Files: tools/splaytree.js tools/codemap.js tools/consarray.js tools/profile.js


function stackToString(stack) {
  return stack.join(' -> ');
};


function assertPathExists(root, path, opt_message) {
  var message = opt_message ? ' (' + opt_message + ')' : '';
  assertNotNull(root.descendToChild(path, function(node, pos) {
    assertNotNull(node,
      stackToString(path.slice(0, pos)) + ' has no child ' +
                    path[pos] + message);
  }), opt_message);
};


function assertNoPathExists(root, path, opt_message) {
  var message = opt_message ? ' (' + opt_message + ')' : '';
  assertNull(root.descendToChild(path), opt_message);
};


function countNodes(profile, traverseFunc) {
  var count = 0;
  traverseFunc.call(profile, function () { count++; });
  return count;
};


function ProfileTestDriver() {
  this.profile = new Profile();
  this.stack_ = [];
  this.addFunctions_();
};


// Addresses inside functions.
ProfileTestDriver.prototype.funcAddrs_ = {
    'lib1-f1': 0x11110, 'lib1-f2': 0x11210,
    'lib2-f1': 0x21110, 'lib2-f2': 0x21210,
    'T: F1': 0x50110, 'T: F2': 0x50210, 'T: F3': 0x50410 };


ProfileTestDriver.prototype.addFunctions_ = function() {
  this.profile.addLibrary('lib1', 0x11000, 0x12000);
  this.profile.addStaticCode('lib1-f1', 0x11100, 0x11900);
  this.profile.addStaticCode('lib1-f2', 0x11200, 0x11500);
  this.profile.addLibrary('lib2', 0x21000, 0x22000);
  this.profile.addStaticCode('lib2-f1', 0x21100, 0x21900);
  this.profile.addStaticCode('lib2-f2', 0x21200, 0x21500);
  this.profile.addCode('T', 'F1', 0x50100, 0x100);
  this.profile.addCode('T', 'F2', 0x50200, 0x100);
  this.profile.addCode('T', 'F3', 0x50400, 0x100);
};


ProfileTestDriver.prototype.enter = function(funcName) {
  // Stack looks like this: [pc, caller, ..., main].
  // Therefore, we are adding entries at the beginning.
  this.stack_.unshift(this.funcAddrs_[funcName]);
  this.profile.recordTick(this.stack_);
};


ProfileTestDriver.prototype.stay = function() {
  this.profile.recordTick(this.stack_);
};


ProfileTestDriver.prototype.leave = function() {
  this.stack_.shift();
};


ProfileTestDriver.prototype.execute = function() {
  this.enter('lib1-f1');
    this.enter('lib1-f2');
      this.enter('T: F1');
        this.enter('T: F2');
        this.leave();
      this.stay();
        this.enter('lib2-f1');
          this.enter('lib2-f1');
          this.leave();
        this.stay();
        this.leave();
        this.enter('T: F3');
          this.enter('T: F3');
            this.enter('T: F3');
            this.leave();
            this.enter('T: F2');
            this.stay();
            this.leave();
          this.leave();
        this.leave();
      this.leave();
      this.enter('lib2-f1');
        this.enter('lib1-f1');
        this.leave();
      this.leave();
    this.stay();
  this.leave();
};


function Inherits(childCtor, parentCtor) {
  function tempCtor() {};
  tempCtor.prototype = parentCtor.prototype;
  childCtor.superClass_ = parentCtor.prototype;
  childCtor.prototype = new tempCtor();
  childCtor.prototype.constructor = childCtor;
};


(function testCallTreeBuilding() {
  function Driver() {
    ProfileTestDriver.call(this);
    this.namesTopDown = [];
    this.namesBottomUp = [];
  };
  Inherits(Driver, ProfileTestDriver);

  Driver.prototype.enter = function(func) {
    this.namesTopDown.push(func);
    this.namesBottomUp.unshift(func);
    assertNoPathExists(this.profile.getTopDownProfile().getRoot(), this.namesTopDown,
        'pre enter/topDown');
    assertNoPathExists(this.profile.getBottomUpProfile().getRoot(), this.namesBottomUp,
        'pre enter/bottomUp');
    Driver.superClass_.enter.call(this, func);
    assertPathExists(this.profile.getTopDownProfile().getRoot(), this.namesTopDown,
        'post enter/topDown');
    assertPathExists(this.profile.getBottomUpProfile().getRoot(), this.namesBottomUp,
        'post enter/bottomUp');
  };

  Driver.prototype.stay = function() {
    var preTopDownNodes = countNodes(this.profile, this.profile.traverseTopDownTree);
    var preBottomUpNodes = countNodes(this.profile, this.profile.traverseBottomUpTree);
    Driver.superClass_.stay.call(this);
    var postTopDownNodes = countNodes(this.profile, this.profile.traverseTopDownTree);
    var postBottomUpNodes = countNodes(this.profile, this.profile.traverseBottomUpTree);
    // Must be no changes in tree layout.
    assertEquals(preTopDownNodes, postTopDownNodes, 'stay/topDown');
    assertEquals(preBottomUpNodes, postBottomUpNodes, 'stay/bottomUp');
  };

  Driver.prototype.leave = function() {
    Driver.superClass_.leave.call(this);
    this.namesTopDown.pop();
    this.namesBottomUp.shift();
  };

  var testDriver = new Driver();
  testDriver.execute();
})();


function assertNodeWeights(root, path, selfTicks, totalTicks) {
  var node = root.descendToChild(path);
  var stack = stackToString(path);
  assertNotNull(node, 'node not found: ' + stack);
  assertEquals(selfTicks, node.selfWeight, 'self of ' + stack);
  assertEquals(totalTicks, node.totalWeight, 'total of ' + stack);
};


(function testTopDownRootProfileTicks() {
  var testDriver = new ProfileTestDriver();
  testDriver.execute();

  var pathWeights = [
    [['lib1-f1'], 1, 16],
    [['lib1-f1', 'lib1-f2'], 2, 15],
    [['lib1-f1', 'lib1-f2', 'T: F1'], 2, 11],
    [['lib1-f1', 'lib1-f2', 'T: F1', 'T: F2'], 1, 1],
    [['lib1-f1', 'lib1-f2', 'T: F1', 'lib2-f1'], 2, 3],
    [['lib1-f1', 'lib1-f2', 'T: F1', 'lib2-f1', 'lib2-f1'], 1, 1],
    [['lib1-f1', 'lib1-f2', 'T: F1', 'T: F3'], 1, 5],
    [['lib1-f1', 'lib1-f2', 'T: F1', 'T: F3', 'T: F3'], 1, 4],
    [['lib1-f1', 'lib1-f2', 'T: F1', 'T: F3', 'T: F3', 'T: F3'], 1, 1],
    [['lib1-f1', 'lib1-f2', 'T: F1', 'T: F3', 'T: F3', 'T: F2'], 2, 2],
    [['lib1-f1', 'lib1-f2', 'lib2-f1'], 1, 2],
    [['lib1-f1', 'lib1-f2', 'lib2-f1', 'lib1-f1'], 1, 1]
  ];

  var root = testDriver.profile.getTopDownProfile().getRoot();
  for (var i = 0; i < pathWeights.length; ++i) {
    var data = pathWeights[i];
    assertNodeWeights(root, data[0], data[1], data[2]);
  }
})();


(function testRootFlatProfileTicks() {
  function Driver() {
    ProfileTestDriver.call(this);
    this.namesTopDown = [''];
    this.counters = {};
    this.root = null;
  };
  Inherits(Driver, ProfileTestDriver);

  Driver.prototype.increment = function(func, self, total) {
    if (!(func in this.counters)) {
      this.counters[func] = { self: 0, total: 0 };
    }
    this.counters[func].self += self;
    this.counters[func].total += total;
  };

  Driver.prototype.incrementTotals = function() {
    // Only count each function in the stack once.
    var met = {};
    for (var i = 0; i < this.namesTopDown.length; ++i) {
      var name = this.namesTopDown[i];
      if (!(name in met)) {
        this.increment(name, 0, 1);
      }
      met[name] = true;
    }
  };

  Driver.prototype.enter = function(func) {
    Driver.superClass_.enter.call(this, func);
    this.namesTopDown.push(func);
    this.increment(func, 1, 0);
    this.incrementTotals();
  };

  Driver.prototype.stay = function() {
    Driver.superClass_.stay.call(this);
    this.increment(this.namesTopDown[this.namesTopDown.length - 1], 1, 0);
    this.incrementTotals();
  };

  Driver.prototype.leave = function() {
    Driver.superClass_.leave.call(this);
    this.namesTopDown.pop();
  };

  Driver.prototype.extractRoot = function() {
    assertTrue('' in this.counters);
    this.root = this.counters[''];
    delete this.counters[''];
  };

  var testDriver = new Driver();
  testDriver.execute();
  testDriver.extractRoot();

  var counted = 0;
  for (var c in testDriver.counters) {
    counted++;
  }

  var flatProfileRoot = testDriver.profile.getFlatProfile().getRoot();
  assertEquals(testDriver.root.self, flatProfileRoot.selfWeight);
  assertEquals(testDriver.root.total, flatProfileRoot.totalWeight);

  var flatProfile = flatProfileRoot.exportChildren();
  assertEquals(counted, flatProfile.length, 'counted vs. flatProfile');
  for (var i = 0; i < flatProfile.length; ++i) {
    var rec = flatProfile[i];
    assertTrue(rec.label in testDriver.counters, 'uncounted: ' + rec.label);
    var reference = testDriver.counters[rec.label];
    assertEquals(reference.self, rec.selfWeight, 'self of ' + rec.label);
    assertEquals(reference.total, rec.totalWeight, 'total of ' + rec.label);
  }

})();


(function testFunctionCalleesProfileTicks() {
  var testDriver = new ProfileTestDriver();
  testDriver.execute();

  var pathWeights = [
    [['lib2-f1'], 3, 5],
    [['lib2-f1', 'lib2-f1'], 1, 1],
    [['lib2-f1', 'lib1-f1'], 1, 1]
  ];

  var profile = testDriver.profile.getTopDownProfile('lib2-f1');
  var root = profile.getRoot();
  for (var i = 0; i < pathWeights.length; ++i) {
    var data = pathWeights[i];
    assertNodeWeights(root, data[0], data[1], data[2]);
  }
})();


(function testFunctionFlatProfileTicks() {
  var testDriver = new ProfileTestDriver();
  testDriver.execute();

  var flatWeights = {
    'lib2-f1': [1, 1],
    'lib1-f1': [1, 1]
  };

  var flatProfileRoot =
     testDriver.profile.getFlatProfile('lib2-f1').findOrAddChild('lib2-f1');
  assertEquals(3, flatProfileRoot.selfWeight);
  assertEquals(5, flatProfileRoot.totalWeight);

  var flatProfile = flatProfileRoot.exportChildren();
  assertEquals(2, flatProfile.length, 'counted vs. flatProfile');
  for (var i = 0; i < flatProfile.length; ++i) {
    var rec = flatProfile[i];
    assertTrue(rec.label in flatWeights, 'uncounted: ' + rec.label);
    var reference = flatWeights[rec.label];
    assertEquals(reference[0], rec.selfWeight, 'self of ' + rec.label);
    assertEquals(reference[1], rec.totalWeight, 'total of ' + rec.label);
  }

})();

