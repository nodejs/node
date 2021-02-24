// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Regression test to ensure no Intl["SegmentIterator"]

assertThrows(() => new Intl["SegmentIterator"](), TypeError);
