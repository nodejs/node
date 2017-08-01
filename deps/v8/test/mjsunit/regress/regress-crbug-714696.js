// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (this.Intl) {
  new Intl.v8BreakIterator();
  new Intl.DateTimeFormat();
  console.log({ toString: function() { throw 1; }});
  new Intl.v8BreakIterator();
}
