// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var target = Object.create(null);
var proxy = new Proxy(target, {
  ownKeys: function() {
    return ['a'];
  }
});
for (var key in proxy) ;
