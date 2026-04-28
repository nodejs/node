// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertThrows(() =>
    Object.getOwnPropertyDescriptor(Intl.Collator.prototype, 'compare')
      .get.call(new Intl.DateTimeFormat())('a', 'b'),
    TypeError);
