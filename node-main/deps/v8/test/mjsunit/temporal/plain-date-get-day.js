// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

let d1 = new Temporal.PlainDate(2021, 12, 11,
    {day: function(like) {return 2;}});

assertEquals(2, d1.day);
