// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var log = [];

var fake =
    {
      get source() {
        log.push("p");
        return {
          toString: function() {
            log.push("ps");
            return "pattern";
          }
        };
      },
      get flags() {
        log.push("f");
        return {
          toString: function() {
            log.push("fs");
            return "flags";
          }
        };
      }
    }

function testThrows(x) {
  try {
    RegExp.prototype.toString.call(x);
  } catch (e) {
    assertTrue(/incompatible receiver/.test(e.message));
    return;
  }
  assertUnreachable();
}

testThrows(1);
testThrows(null);
Number.prototype.source = "a";
Number.prototype.flags = "b";
testThrows(1);

assertEquals("/pattern/flags", RegExp.prototype.toString.call(fake));
assertEquals(["p", "ps", "f", "fs"], log);

// Monkey-patching is also possible on RegExp instances

let weird = /foo/;
Object.defineProperty(weird, 'flags', {value: 'bar'});
Object.defineProperty(weird, 'source', {value: 'baz'});
assertEquals('/baz/bar', weird.toString());

assertEquals('/(?:)/', RegExp.prototype.toString());
assertEquals('(?:)', RegExp.prototype.source);
assertEquals('', RegExp.prototype.flags);
