// Copyright 2014 the V8 project authors. All rights reserved.
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

// This file emulates Mocha test framework used in promises-aplus tests.

var describe;
var it;
var specify;
var before;
var after;
var beforeEach;
var afterEach;
var RunAllTests;

var assert = require('assert');

(function() {
var TIMEOUT = 1000;

var context = {
  beingDescribed: undefined,
  currentSuiteIndex: 0,
  suites: []
};

function Run() {
  function current() {
    while (context.currentSuiteIndex < context.suites.length &&
           context.suites[context.currentSuiteIndex].hasRun) {
      ++context.currentSuiteIndex;
    }
    if (context.suites.length == context.currentSuiteIndex) {
      return undefined;
    }
    return context.suites[context.currentSuiteIndex];
  }
  var suite = current();
  if (!suite) {
    // done
    print('All tests have run.');
    return;
  }
  suite.Run();
}

RunAllTests = function() {
  context.currentSuiteIndex = 0;
  var numRegularTestCases = 0;
  for (var i = 0; i < context.suites.length; ++i) {
    numRegularTestCases += context.suites[i].numRegularTestCases();
  }
  print(context.suites.length + ' suites and ' + numRegularTestCases +
        ' test cases are found');
  Run();
};

function TestCase(name, before, fn, after, isRegular) {
  this.name = name;
  this.before = before;
  this.fn = fn;
  this.after = after;
  this.isRegular = isRegular;
  this.hasDone = false;
}

TestCase.prototype.RunFunction = function(suite, fn, postAction) {
  if (!fn) {
    postAction();
    return;
  }
  try {
    if (fn.length === 0) {
      // synchronous
      fn();
      postAction();
    } else {
      // asynchronous
      fn(postAction);
    }
  } catch (e) {
    suite.ReportError(this, e);
  }
}

TestCase.prototype.MarkAsDone = function() {
  this.hasDone = true;
  clearTimeout(this.timer);
}

TestCase.prototype.Run = function(suite, postAction) {
  print('Running ' + suite.description + '#' + this.name + ' ...');
  assert.clear();

  this.timer = setTimeout(function() {
    suite.ReportError(this, Error('timeout'));
  }.bind(this), TIMEOUT);

  this.RunFunction(suite, this.before, function(e) {
    if (this.hasDone) {
      return;
    }
    if (e instanceof Error) {
      return suite.ReportError(this, e);
    }
    if (assert.fails.length > 0) {
      return suite.ReportError(this, assert.fails[0]);
    }
    this.RunFunction(suite, this.fn, function(e) {
      if (this.hasDone) {
        return;
      }
      if (e instanceof Error) {
        return suite.ReportError(this, e);
      }
      if (assert.fails.length > 0) {
        return suite.ReportError(this, assert.fails[0]);
      }
      this.RunFunction(suite, this.after, function(e) {
        if (this.hasDone) {
          return;
        }
        if (e instanceof Error) {
          return suite.ReportError(this, e);
        }
        if (assert.fails.length > 0) {
          return suite.ReportError(this, assert.fails[0]);
        }
        this.MarkAsDone();
        if (this.isRegular) {
          print('PASS: ' + suite.description + '#' + this.name);
        }
        %EnqueueMicrotask(postAction);
      }.bind(this));
    }.bind(this));
  }.bind(this));
};

function TestSuite(described) {
  this.description = described.description;
  this.cases = [];
  this.currentIndex = 0;
  this.hasRun = false;

  if (described.before) {
    this.cases.push(new TestCase(this.description + ' :before', undefined,
                                 described.before, undefined, false));
  }
  for (var i = 0; i < described.cases.length; ++i) {
    this.cases.push(new TestCase(described.cases[i].description,
                                 described.beforeEach,
                                 described.cases[i].fn,
                                 described.afterEach,
                                 true));
  }
  if (described.after) {
    this.cases.push(new TestCase(this.description + ' :after',
                                 undefined, described.after, undefined, false));
  }
}

TestSuite.prototype.Run = function() {
  this.hasRun = this.currentIndex === this.cases.length;
  if (this.hasRun) {
    %EnqueueMicrotask(Run);
    return;
  }

  // TestCase.prototype.Run cannot throw an exception.
  this.cases[this.currentIndex].Run(this, function() {
    ++this.currentIndex;
    %EnqueueMicrotask(Run);
  }.bind(this));
};

TestSuite.prototype.numRegularTestCases = function() {
  var n = 0;
  for (var i = 0; i < this.cases.length; ++i) {
    if (this.cases[i].isRegular) {
      ++n;
    }
  }
  return n;
}

TestSuite.prototype.ReportError = function(testCase, e) {
  if (testCase.hasDone) {
    return;
  }
  testCase.MarkAsDone();
  this.hasRun = this.currentIndex === this.cases.length;
  print('FAIL: ' + this.description + '#' + testCase.name + ': ' +
        e.name  + ' (' + e.message + ')');
  ++this.currentIndex;
  %EnqueueMicrotask(Run);
};

describe = function(description, fn) {
  var parent = context.beingDescribed;
  var incomplete = {
    cases: [],
    description: parent ? parent.description + ' ' + description : description,
    parent: parent,
  };
  context.beingDescribed = incomplete;
  fn();
  context.beingDescribed = parent;

  context.suites.push(new TestSuite(incomplete));
}

specify = it = function(description, fn) {
  context.beingDescribed.cases.push({description: description, fn: fn});
}

before = function(fn) {
  context.beingDescribed.before = fn;
}

after = function(fn) {
  context.beingDescribed.after = fn;
}

beforeEach = function(fn) {
  context.beingDescribed.beforeEach = fn;
}

afterEach = function(fn) {
  context.beingDescribed.afterEach = fn;
}

}());
