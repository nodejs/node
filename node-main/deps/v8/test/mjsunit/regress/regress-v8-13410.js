// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Make sure lone surrogates don't match combined surrogates.
assertFalse(/[\ud800-\udfff]+/u.test('\ud801\udc0f'));

// Surrogate pairs split by an "always succeeding" backref shouldn't match
// combined surrogates.
assertFalse(/(\ud801\1\udc0f)/u.test('\ud801\udc0f'));
assertFalse(/(\ud801\1?\udc0f)/u.test('\ud801\udc0f'));
assertFalse(/(\ud801\1{0}\udc0f)/u.test('\ud801\udc0f'));
assertFalse(new RegExp('(\ud801\\1\udc0f)','u').test('\ud801\udc0f'));
assertFalse(new RegExp('(\ud801\\1?\udc0f)','u').test('\ud801\udc0f'));
assertFalse(new RegExp('(\ud801\\1{0}\udc0f)','u').test('\ud801\udc0f'));
