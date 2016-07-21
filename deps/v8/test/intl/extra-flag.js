// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --intl-extra

// Turning on the creates the non-standard properties

var dtf = new Intl.DateTimeFormat(['en']);
assertTrue('v8Parse' in dtf);
assertTrue('resolved' in dtf);
assertTrue(!!dtf.resolved && 'pattern' in dtf.resolved);

var nf = new Intl.NumberFormat(['en']);
assertTrue('v8Parse' in nf);
assertTrue('resolved' in nf);
assertTrue(!!nf.resolved && 'pattern' in nf.resolved);

var col = new Intl.Collator(['en']);
assertTrue('resolved' in col);

var br = new Intl.v8BreakIterator(['en']);
assertTrue('resolved' in br);
