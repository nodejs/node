// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var nre = /[^!]$/;
var nre_u = /[^!]$/u;
var nre_v = /[^!]$/v;
var nre_i = /[^!]$/i;
var nre_iu = /[^!]$/iu;
var nre_iv = /[^!]$/iv;

assertTrue(nre.test('\u{1F4A9}'))
assertTrue(nre_u.test('\u{1F4A9}'))
assertTrue(nre_v.test('\u{1F4A9}'))
assertTrue(nre_i.test('\u{1F4A9}'))
assertTrue(nre_iu.test('\u{1F4A9}'))
assertTrue(nre_iv.test('\u{1F4A9}'))

var re = /[!]$/;
var re_u = /[!]$/u;
var re_v = /[!]$/v;
var re_i = /[!]$/i;
var re_iu = /[!]$/iu;
var re_iv = /[!]$/iv;

assertFalse(re.test('\u{1F4A9}'))
assertFalse(re_u.test('\u{1F4A9}'))
assertFalse(re_v.test('\u{1F4A9}'))
assertFalse(re_i.test('\u{1F4A9}'))
assertFalse(re_iu.test('\u{1F4A9}'))
assertFalse(re_iv.test('\u{1F4A9}'))
