// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// a has packed SMI elements
a = [];

// a has dictionary elements
a.length = 0xFFFFFFF;

// a has dictionary elements and the backing array is
// empty_slow_element_dictionary (length 0)
a.length = 0;

// a has dictionary elements and the backing array is
// empty_slow_element_dictionary (length 0xFFFFFFF)
a.length = 0xFFFFFFF;

// This will crash if V8 attempts to remove 0 elements from
// empty_slow_element_dictionary as it is in RO_SPACE.
a.length = 1;
