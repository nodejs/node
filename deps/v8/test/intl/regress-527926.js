// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let date = new Date(2015, 8, 1, 3, 0, 0);
var fmt = new Intl.DateTimeFormat('ru', {hour:'2-digit', minute: '2-digit'})
assertEquals("03:00", fmt.format(date));

fmt = new Intl.DateTimeFormat(
    'en', {hour:'2-digit', minute: '2-digit', hour12: false});
assertEquals("03:00", fmt.format(date));

fmt = new Intl.DateTimeFormat(
    'ru', {hour:'2-digit', minute: '2-digit', hour12: false});
assertEquals("03:00", fmt.format(date));

fmt = new Intl.DateTimeFormat(
    'ru', {hour:'2-digit', minute: '2-digit', hour12: true});
assertEquals("03:00 AM", fmt.format(date));
