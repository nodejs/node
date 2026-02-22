// META: global=window,worker

"use strict";

var log = [];
function clearLog() {
  log = [];
}
function addLogEntry(name, args) {
  log.push([ name, ...args ]);
}

var loggingHandler = {
};

setup(function() {
  for (let prop of Object.getOwnPropertyNames(Reflect)) {
    loggingHandler[prop] = function(...args) {
      addLogEntry(prop, args);
      return Reflect[prop](...args);
    }
  }
});

test(function() {
  var h = new Headers();
  assert_equals([...h].length, 0);
}, "Passing nothing to Headers constructor");

test(function() {
  var h = new Headers(undefined);
  assert_equals([...h].length, 0);
}, "Passing undefined to Headers constructor");

test(function() {
  assert_throws_js(TypeError, function() {
    var h = new Headers(null);
  });
}, "Passing null to Headers constructor");

test(function() {
  this.add_cleanup(clearLog);
  var record = { a: "b" };
  var proxy = new Proxy(record, loggingHandler);
  var h = new Headers(proxy);

  assert_equals(log.length, 4);
  // The first thing is the [[Get]] of Symbol.iterator to figure out whether
  // we're a sequence, during overload resolution.
  assert_array_equals(log[0], ["get", record, Symbol.iterator, proxy]);
  // Then we have the [[OwnPropertyKeys]] from
  // https://webidl.spec.whatwg.org/#es-to-record step 4.
  assert_array_equals(log[1], ["ownKeys", record]);
  // Then the [[GetOwnProperty]] from step 5.1.
  assert_array_equals(log[2], ["getOwnPropertyDescriptor", record, "a"]);
  // Then the [[Get]] from step 5.2.
  assert_array_equals(log[3], ["get", record, "a", proxy]);

  // Check the results.
  assert_equals([...h].length, 1);
  assert_array_equals([...h.keys()], ["a"]);
  assert_true(h.has("a"));
  assert_equals(h.get("a"), "b");
}, "Basic operation with one property");

test(function() {
  this.add_cleanup(clearLog);
  var recordProto = { c: "d" };
  var record = Object.create(recordProto, { a: { value: "b", enumerable: true } });
  var proxy = new Proxy(record, loggingHandler);
  var h = new Headers(proxy);

  assert_equals(log.length, 4);
  // The first thing is the [[Get]] of Symbol.iterator to figure out whether
  // we're a sequence, during overload resolution.
  assert_array_equals(log[0], ["get", record, Symbol.iterator, proxy]);
  // Then we have the [[OwnPropertyKeys]] from
  // https://webidl.spec.whatwg.org/#es-to-record step 4.
  assert_array_equals(log[1], ["ownKeys", record]);
  // Then the [[GetOwnProperty]] from step 5.1.
  assert_array_equals(log[2], ["getOwnPropertyDescriptor", record, "a"]);
  // Then the [[Get]] from step 5.2.
  assert_array_equals(log[3], ["get", record, "a", proxy]);

  // Check the results.
  assert_equals([...h].length, 1);
  assert_array_equals([...h.keys()], ["a"]);
  assert_true(h.has("a"));
  assert_equals(h.get("a"), "b");
}, "Basic operation with one property and a proto");

test(function() {
  this.add_cleanup(clearLog);
  var record = { a: "b", c: "d" };
  var proxy = new Proxy(record, loggingHandler);
  var h = new Headers(proxy);

  assert_equals(log.length, 6);
  // The first thing is the [[Get]] of Symbol.iterator to figure out whether
  // we're a sequence, during overload resolution.
  assert_array_equals(log[0], ["get", record, Symbol.iterator, proxy]);
  // Then we have the [[OwnPropertyKeys]] from
  // https://webidl.spec.whatwg.org/#es-to-record step 4.
  assert_array_equals(log[1], ["ownKeys", record]);
  // Then the [[GetOwnProperty]] from step 5.1.
  assert_array_equals(log[2], ["getOwnPropertyDescriptor", record, "a"]);
  // Then the [[Get]] from step 5.2.
  assert_array_equals(log[3], ["get", record, "a", proxy]);
  // Then the second [[GetOwnProperty]] from step 5.1.
  assert_array_equals(log[4], ["getOwnPropertyDescriptor", record, "c"]);
  // Then the second [[Get]] from step 5.2.
  assert_array_equals(log[5], ["get", record, "c", proxy]);

  // Check the results.
  assert_equals([...h].length, 2);
  assert_array_equals([...h.keys()], ["a", "c"]);
  assert_true(h.has("a"));
  assert_equals(h.get("a"), "b");
  assert_true(h.has("c"));
  assert_equals(h.get("c"), "d");
}, "Correct operation ordering with two properties");

test(function() {
  this.add_cleanup(clearLog);
  var record = { a: "b", "\uFFFF": "d" };
  var proxy = new Proxy(record, loggingHandler);
  assert_throws_js(TypeError, function() {
    var h = new Headers(proxy);
  });

  assert_equals(log.length, 5);
  // The first thing is the [[Get]] of Symbol.iterator to figure out whether
  // we're a sequence, during overload resolution.
  assert_array_equals(log[0], ["get", record, Symbol.iterator, proxy]);
  // Then we have the [[OwnPropertyKeys]] from
  // https://webidl.spec.whatwg.org/#es-to-record step 4.
  assert_array_equals(log[1], ["ownKeys", record]);
  // Then the [[GetOwnProperty]] from step 5.1.
  assert_array_equals(log[2], ["getOwnPropertyDescriptor", record, "a"]);
  // Then the [[Get]] from step 5.2.
  assert_array_equals(log[3], ["get", record, "a", proxy]);
  // Then the second [[GetOwnProperty]] from step 5.1.
  assert_array_equals(log[4], ["getOwnPropertyDescriptor", record, "\uFFFF"]);
  // The second [[Get]] never happens, because we convert the invalid name to a
  // ByteString first and throw.
}, "Correct operation ordering with two properties one of which has an invalid name");

test(function() {
  this.add_cleanup(clearLog);
  var record = { a: "\uFFFF", c: "d" }
  var proxy = new Proxy(record, loggingHandler);
  assert_throws_js(TypeError, function() {
    var h = new Headers(proxy);
  });

  assert_equals(log.length, 4);
  // The first thing is the [[Get]] of Symbol.iterator to figure out whether
  // we're a sequence, during overload resolution.
  assert_array_equals(log[0], ["get", record, Symbol.iterator, proxy]);
  // Then we have the [[OwnPropertyKeys]] from
  // https://webidl.spec.whatwg.org/#es-to-record step 4.
  assert_array_equals(log[1], ["ownKeys", record]);
  // Then the [[GetOwnProperty]] from step 5.1.
  assert_array_equals(log[2], ["getOwnPropertyDescriptor", record, "a"]);
  // Then the [[Get]] from step 5.2.
  assert_array_equals(log[3], ["get", record, "a", proxy]);
  // Nothing else after this, because converting the result of that [[Get]] to a
  // ByteString throws.
}, "Correct operation ordering with two properties one of which has an invalid value");

test(function() {
  this.add_cleanup(clearLog);
  var record = {};
  Object.defineProperty(record, "a", { value: "b", enumerable: false });
  Object.defineProperty(record, "c", { value: "d", enumerable: true });
  Object.defineProperty(record, "e", { value: "f", enumerable: false });
  var proxy = new Proxy(record, loggingHandler);
  var h = new Headers(proxy);

  assert_equals(log.length, 6);
  // The first thing is the [[Get]] of Symbol.iterator to figure out whether
  // we're a sequence, during overload resolution.
  assert_array_equals(log[0], ["get", record, Symbol.iterator, proxy]);
  // Then we have the [[OwnPropertyKeys]] from
  // https://webidl.spec.whatwg.org/#es-to-record step 4.
  assert_array_equals(log[1], ["ownKeys", record]);
  // Then the [[GetOwnProperty]] from step 5.1.
  assert_array_equals(log[2], ["getOwnPropertyDescriptor", record, "a"]);
  // No [[Get]] because not enumerable
  // Then the second [[GetOwnProperty]] from step 5.1.
  assert_array_equals(log[3], ["getOwnPropertyDescriptor", record, "c"]);
  // Then the [[Get]] from step 5.2.
  assert_array_equals(log[4], ["get", record, "c", proxy]);
  // Then the third [[GetOwnProperty]] from step 5.1.
  assert_array_equals(log[5], ["getOwnPropertyDescriptor", record, "e"]);
  // No [[Get]] because not enumerable

  // Check the results.
  assert_equals([...h].length, 1);
  assert_array_equals([...h.keys()], ["c"]);
  assert_true(h.has("c"));
  assert_equals(h.get("c"), "d");
}, "Correct operation ordering with non-enumerable properties");

test(function() {
  this.add_cleanup(clearLog);
  var record = {a: "b", c: "d", e: "f"};
  var lyingHandler = {
    getOwnPropertyDescriptor: function(target, name) {
      if (name == "a" || name == "e") {
        return undefined;
      }
      return Reflect.getOwnPropertyDescriptor(target, name);
    }
  };
  var lyingProxy = new Proxy(record, lyingHandler);
  var proxy = new Proxy(lyingProxy, loggingHandler);
  var h = new Headers(proxy);

  assert_equals(log.length, 6);
  // The first thing is the [[Get]] of Symbol.iterator to figure out whether
  // we're a sequence, during overload resolution.
  assert_array_equals(log[0], ["get", lyingProxy, Symbol.iterator, proxy]);
  // Then we have the [[OwnPropertyKeys]] from
  // https://webidl.spec.whatwg.org/#es-to-record step 4.
  assert_array_equals(log[1], ["ownKeys", lyingProxy]);
  // Then the [[GetOwnProperty]] from step 5.1.
  assert_array_equals(log[2], ["getOwnPropertyDescriptor", lyingProxy, "a"]);
  // No [[Get]] because no descriptor
  // Then the second [[GetOwnProperty]] from step 5.1.
  assert_array_equals(log[3], ["getOwnPropertyDescriptor", lyingProxy, "c"]);
  // Then the [[Get]] from step 5.2.
  assert_array_equals(log[4], ["get", lyingProxy, "c", proxy]);
  // Then the third [[GetOwnProperty]] from step 5.1.
  assert_array_equals(log[5], ["getOwnPropertyDescriptor", lyingProxy, "e"]);
  // No [[Get]] because no descriptor

  // Check the results.
  assert_equals([...h].length, 1);
  assert_array_equals([...h.keys()], ["c"]);
  assert_true(h.has("c"));
  assert_equals(h.get("c"), "d");
}, "Correct operation ordering with undefined descriptors");

test(function() {
  this.add_cleanup(clearLog);
  var record = {a: "b", c: "d"};
  var lyingHandler = {
    ownKeys: function() {
      return [ "a", "c", "a", "c" ];
    },
  };
  var lyingProxy = new Proxy(record, lyingHandler);
  var proxy = new Proxy(lyingProxy, loggingHandler);

  // Returning duplicate keys from ownKeys() throws a TypeError.
  assert_throws_js(TypeError,
                   function() { var h = new Headers(proxy); });

  assert_equals(log.length, 2);
  // The first thing is the [[Get]] of Symbol.iterator to figure out whether
  // we're a sequence, during overload resolution.
  assert_array_equals(log[0], ["get", lyingProxy, Symbol.iterator, proxy]);
  // Then we have the [[OwnPropertyKeys]] from
  // https://webidl.spec.whatwg.org/#es-to-record step 4.
  assert_array_equals(log[1], ["ownKeys", lyingProxy]);
}, "Correct operation ordering with repeated keys");

test(function() {
  this.add_cleanup(clearLog);
  var record = {
    a: "b",
    [Symbol.toStringTag]: {
      // Make sure the ToString conversion of the value happens
      // after the ToString conversion of the key.
      toString: function () { addLogEntry("toString", [this]); return "nope"; }
    },
    c: "d" };
  var proxy = new Proxy(record, loggingHandler);
  assert_throws_js(TypeError,
                   function() { var h = new Headers(proxy); });

  assert_equals(log.length, 7);
  // The first thing is the [[Get]] of Symbol.iterator to figure out whether
  // we're a sequence, during overload resolution.
  assert_array_equals(log[0], ["get", record, Symbol.iterator, proxy]);
  // Then we have the [[OwnPropertyKeys]] from
  // https://webidl.spec.whatwg.org/#es-to-record step 4.
  assert_array_equals(log[1], ["ownKeys", record]);
  // Then the [[GetOwnProperty]] from step 5.1.
  assert_array_equals(log[2], ["getOwnPropertyDescriptor", record, "a"]);
  // Then the [[Get]] from step 5.2.
  assert_array_equals(log[3], ["get", record, "a", proxy]);
  // Then the second [[GetOwnProperty]] from step 5.1.
  assert_array_equals(log[4], ["getOwnPropertyDescriptor", record, "c"]);
  // Then the second [[Get]] from step 5.2.
  assert_array_equals(log[5], ["get", record, "c", proxy]);
  // Then the third [[GetOwnProperty]] from step 5.1.
  assert_array_equals(log[6], ["getOwnPropertyDescriptor", record,
                               Symbol.toStringTag]);
  // Then we throw an exception converting the Symbol to a string, before we do
  // the third [[Get]].
}, "Basic operation with Symbol keys");

test(function() {
  this.add_cleanup(clearLog);
  var record = {
    a: {
      toString: function() { addLogEntry("toString", [this]); return "b"; }
    },
    [Symbol.toStringTag]: {
      toString: function () { addLogEntry("toString", [this]); return "nope"; }
    },
    c: {
      toString: function() { addLogEntry("toString", [this]); return "d"; }
    }
  };
  // Now make that Symbol-named property not enumerable.
  Object.defineProperty(record, Symbol.toStringTag, { enumerable: false });
  assert_array_equals(Reflect.ownKeys(record),
                      ["a", "c", Symbol.toStringTag]);

  var proxy = new Proxy(record, loggingHandler);
  var h = new Headers(proxy);

  assert_equals(log.length, 9);
  // The first thing is the [[Get]] of Symbol.iterator to figure out whether
  // we're a sequence, during overload resolution.
  assert_array_equals(log[0], ["get", record, Symbol.iterator, proxy]);
  // Then we have the [[OwnPropertyKeys]] from
  // https://webidl.spec.whatwg.org/#es-to-record step 4.
  assert_array_equals(log[1], ["ownKeys", record]);
  // Then the [[GetOwnProperty]] from step 5.1.
  assert_array_equals(log[2], ["getOwnPropertyDescriptor", record, "a"]);
  // Then the [[Get]] from step 5.2.
  assert_array_equals(log[3], ["get", record, "a", proxy]);
  // Then the ToString on the value.
  assert_array_equals(log[4], ["toString", record.a]);
  // Then the second [[GetOwnProperty]] from step 5.1.
  assert_array_equals(log[5], ["getOwnPropertyDescriptor", record, "c"]);
  // Then the second [[Get]] from step 5.2.
  assert_array_equals(log[6], ["get", record, "c", proxy]);
  // Then the ToString on the value.
  assert_array_equals(log[7], ["toString", record.c]);
  // Then the third [[GetOwnProperty]] from step 5.1.
  assert_array_equals(log[8], ["getOwnPropertyDescriptor", record,
                               Symbol.toStringTag]);
  // No [[Get]] because not enumerable.

  // Check the results.
  assert_equals([...h].length, 2);
  assert_array_equals([...h.keys()], ["a", "c"]);
  assert_true(h.has("a"));
  assert_equals(h.get("a"), "b");
  assert_true(h.has("c"));
  assert_equals(h.get("c"), "d");
}, "Operation with non-enumerable Symbol keys");
