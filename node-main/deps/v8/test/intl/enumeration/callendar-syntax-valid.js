// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test the return items of calendar fit 'type'
let regex = /^[a-zA-Z0-9]{3,8}(-[a-zA-Z0-9]{3,8})*$/;
Intl.supportedValuesOf("calendar").forEach(
    function(calendar) {
      assertTrue(regex.test(calendar),
          "Intl.supportedValuesOf('calendar') return " + calendar +
          " which does not meet 'type: alphanum{3,8}(sep alphanum{3,8})*'");
    });
