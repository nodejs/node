// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test the return items of currency is sorted
let name = "currency";
let items = Intl.supportedValuesOf(name);
assertEquals([...items].sort(), items,
      "return value of Intl.supportedValuesOf('" + name + "') should be sorted");
