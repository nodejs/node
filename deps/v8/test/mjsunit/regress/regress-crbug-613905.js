// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Error.prepareStackTrace = (e,s) => s;
var CallSiteConstructor = Error().stack[0].constructor;

try {
  (new CallSiteConstructor(3, 6)).toString();
} catch (e) {
}
