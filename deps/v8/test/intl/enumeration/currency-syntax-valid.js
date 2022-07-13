// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test the return items of currency fit 'type'
let regex = /^[A-Z]{3}$/;
Intl.supportedValuesOf("currency").forEach(
    function(currency) {
      assertTrue(regex.test(currency),
          "Intl.supportedValuesOf('currency') return " + currency +
          " which does not meet 'alpha{3}'");
    });
