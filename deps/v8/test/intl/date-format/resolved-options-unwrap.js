// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test the Intl.DateTimeFormat.prototype.resolvedOptions will properly handle
// 3. Let dtf be ? UnwrapDateTimeFormat(dtf).
var x = Object.create(Intl.DateTimeFormat.prototype);
x = Intl.DateTimeFormat.call(x, 'en');

var resolvedOptions = Intl.DateTimeFormat.prototype.resolvedOptions.call(x);
assertEquals(resolvedOptions.locale, 'en')
