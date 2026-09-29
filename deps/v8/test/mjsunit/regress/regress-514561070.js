// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-inspector

Array.prototype.__defineGetter__(0, function() {
  return new Number();
});
send(JSON.stringify({
  id: 1,
  method: 'Runtime.evaluate',
  params: {includeCommandLineAPI: true, expression: ''}
}));
