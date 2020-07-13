// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const access_log = [];
const handler = {
  get: function(obj, prop) {
    access_log.push(prop);
    return prop in obj ? obj[prop] : "z";
  }
};

class ProxiedGroupRegExp extends RegExp {
  exec(s) {
    var result = super.exec(s);
    if (result) {
      result.groups = new Proxy(result.groups, handler);
    }
    return result;
  }
}

let re = new ProxiedGroupRegExp("(?<x>.)");
assertEquals("a z", "a".replace(re, "$<x> $<y>"));
assertEquals(["x", "y"], access_log);
