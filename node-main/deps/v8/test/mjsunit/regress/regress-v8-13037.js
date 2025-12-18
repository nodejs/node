// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var r = Proxy.revocable({}, {});
r.revoke();
assertThrows(() => {
  Object.prototype.toString.call(r.proxy)
}, TypeError, "Cannot perform 'Object.prototype.toString' on a proxy that has been revoked");
