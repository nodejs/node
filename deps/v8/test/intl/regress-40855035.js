// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const formatter = new Intl.DateTimeFormat('en-us',{minute: '2-digit'})
assertEquals("02", formatter.format(new Date(2024, 6, 21, 3, 2, 0)));
