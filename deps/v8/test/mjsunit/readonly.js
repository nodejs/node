// Copyright 2011 the V8 project authors. All rights reserved.
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

// Flags: --allow-natives-syntax
// This test manually triggers optimization, no need for stress modes.
// Flags: --nostress-opt --noalways-opt

// Different ways to create an object.

function CreateFromLiteral() {
  return {};
}

function CreateFromObject() {
  return new Object();
}

function CreateDefault() {
  return Object.create(Object.prototype);
}

function CreateFromConstructor(proto) {
  function C() {}
  new C().b = 9;  // Make sure that we can have an in-object property.
  C.prototype = proto;
  return function() {
    return new C();
  };
}

function CreateFromApi(proto) {
  return function() {
    return Object.create(proto);
  };
}

function CreateWithProperty(proto) {
  function C() {
    this.a = -100;
  }
  C.prototype = proto;
  return function() {
    return new C();
  };
}

var bases = [CreateFromLiteral, CreateFromObject, CreateDefault];
var inherits = [CreateFromConstructor, CreateFromApi, CreateWithProperty];
var constructs = [CreateFromConstructor, CreateFromApi];

function TestAllCreates(f) {
  // The depth of the prototype chain up the.
  for (var depth = 0; depth < 3; ++depth) {
    // Introduce readonly-ness this far up the chain.
    for (var up = 0; up <= depth; ++up) {
      // Try different construction methods.
      for (var k = 0; k < constructs.length; ++k) {
        // Construct a fresh prototype chain from above functions.
        for (var i = 0; i < bases.length; ++i) {
          var p = bases[i]();
          // There may be a preexisting property under the insertion point...
          for (var j = 0; j < depth - up; ++j) {
            p = inherits[Math.floor(inherits.length * Math.random())](p)();
          }
          // ...but not above it.
          for (var j = 0; j < up; ++j) {
            p = constructs[Math.floor(constructs.length * Math.random())](p)();
          }
          // Create a fresh constructor.
          var c = constructs[k](p);
          f(function() {
            var o = c();
            o.up = o;
            for (var j = 0; j < up; ++j) o.up = Object.getPrototypeOf(o.up);
            return o;
          });
        }
      }
    }
  }
}


// Different ways to make a property read-only.

function ReadonlyByNonwritableDataProperty(o, name) {
  Object.defineProperty(o, name, {value: -41, writable: false});
}

function ReadonlyByAccessorPropertyWithoutSetter(o, name) {
  Object.defineProperty(o, name, {
    get: function() {
      return -42;
    }
  });
}

function ReadonlyByGetter(o, name) {
  o.__defineGetter__('a', function() {
    return -43;
  });
}

function ReadonlyByFreeze(o, name) {
  o[name] = -44;
  Object.freeze(o);
}

function ReadonlyByProto(o, name) {
  var p = Object.create(o.__proto__);
  Object.defineProperty(p, name, {value: -45, writable: false});
  o.__proto__ = p;
}

// TODO(neis,cbruni): Enable once the necessary traps work again.
// Allow Proxy to be undefined, so test can run in non-Harmony mode as well.
var global = this;

function ReadonlyByProxy(o, name) {
  if (!global.Proxy) return ReadonlyByFreeze(o, name);  // Dummy.
  var p = new global.Proxy({}, {
    getPropertyDescriptor: function() {
      return {value: -46, writable: false, configurable: true};
    }
  });

  o.__proto__ = p;
}

var readonlys = [
  ReadonlyByNonwritableDataProperty, ReadonlyByAccessorPropertyWithoutSetter,
  ReadonlyByGetter, ReadonlyByFreeze, ReadonlyByProto  // ReadonlyByProxy
];

function TestAllReadonlys(f) {
  // Provide various methods to making a property read-only.
  for (var i = 0; i < readonlys.length; ++i) {
    print('  readonly =', i);
    f(readonlys[i]);
  }
}


// Different use scenarios.

function Assign(o, x) {
  o.a = x;
};
%PrepareFunctionForOptimization(Assign);
function AssignStrict(o, x) {
  "use strict";
  o.a = x;
};
%PrepareFunctionForOptimization(AssignStrict);
function TestAllModes(f) {
  for (var strict = 0; strict < 2; ++strict) {
    print(" strict =", strict);
    f(strict);
  }
}

function TestAllScenarios(f) {
  for (var t = 0; t < 100; t = 2 * t + 1) {
    print('t =', t);
    f(function(strict, create, readonly) {
      // Make sure that the assignments are monomorphic.
      %DeoptimizeFunction(Assign);
      %DeoptimizeFunction(AssignStrict);
      %ClearFunctionFeedback(Assign);
      %ClearFunctionFeedback(AssignStrict);
      %PrepareFunctionForOptimization(Assign);
      %PrepareFunctionForOptimization(AssignStrict);
      for (var i = 0; i < t; ++i) {
        var o = create();
        assertFalse("a" in o && !("a" in o.__proto__));
        if (strict === 0)
          Assign(o, i);
        else
          AssignStrict(o, i);
        assertEquals(i, o.a);
      }
      %OptimizeFunctionOnNextCall(Assign);
      %OptimizeFunctionOnNextCall(AssignStrict);
      var o = create();
      assertFalse("a" in o && !("a" in o.__proto__));
      readonly(o.up, "a");
      assertTrue("a" in o);
      if (strict === 0)
        Assign(o, t + 1);
      else

        assertThrows(function() {
          AssignStrict(o, t + 1);
        }, TypeError);
      assertTrue(o.a < 0);
    });
  }
}


// Runner.

TestAllScenarios(function(scenario) {
  TestAllModes(function(strict) {
    TestAllReadonlys(function(readonly) {
      TestAllCreates(function(create) {
        scenario(strict, create, readonly);
      });
    });
  });
});

// Extra test forcing bailout.

function Assign2(o, x) {
  o.a = x;
};
%PrepareFunctionForOptimization(Assign2);
(function() {
var p = CreateFromConstructor(Object.prototype)();
var c = CreateFromConstructor(p);
for (var i = 0; i < 3; ++i) {
  var o = c();
  Assign2(o, i);
  assertEquals(i, o.a);
}
%OptimizeFunctionOnNextCall(Assign2);
ReadonlyByNonwritableDataProperty(p, "a");
var o = c();
Assign2(o, 0);
assertTrue(o.a < 0);
})();
