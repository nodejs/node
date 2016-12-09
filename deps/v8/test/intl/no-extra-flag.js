// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-intl-extra

// Turning off the flag removes the non-standard properties

var dtf = new Intl.DateTimeFormat(['en']);
assertFalse('v8Parse' in dtf);
assertFalse('resolved' in dtf);
assertFalse(!!dtf.resolved && 'pattern' in dtf.resolved);

var nf = new Intl.NumberFormat(['en']);
assertFalse('v8Parse' in nf);
assertFalse('resolved' in nf);
assertFalse(!!nf.resolved && 'pattern' in nf.resolved);

var col = new Intl.Collator(['en']);
assertFalse('resolved' in col);

var br = new Intl.v8BreakIterator(['en']);
assertFalse('resolved' in br);
