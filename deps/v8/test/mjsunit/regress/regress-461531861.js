// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const str_with_mu = 'ßµ';
const str_with_umlaut_y = 'ßÿ';
const upper_str_with_mu = str_with_mu.toLocaleUpperCase('de');
const upper_str_with_umlaut_y = str_with_umlaut_y .toLocaleUpperCase('de');
const upper_mu = '\u039c';
const upper_umlaut_y = '\u0178';
assertEquals(`SS${upper_mu}`, upper_str_with_mu);
assertEquals(`SS${upper_umlaut_y}`, upper_str_with_umlaut_y);
