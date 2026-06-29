// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Define an object with a getter and a proxy as it's prototype.
var obj = {foo: 'bar'};
Object.defineProperty(obj, 'foo', {
  get: function () {
  }
});
obj.__proto__ = new Proxy([], {});

// Get key from a function to avoid the property access turning into a
// named property access.
function getKey() {
  return 'values'
}

// Keyed access to update obj's values property.
obj[getKey()] = 1;
