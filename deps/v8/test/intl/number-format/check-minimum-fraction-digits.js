// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Make sure minimumFractionDigits is honored

var nf = new Intl.NumberFormat("en-us",{ useGrouping: false, minimumFractionDigits: 4});

assertEquals("12345.6789", nf.format(12345.6789));
