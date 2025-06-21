// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-intl-locale-info-func

// Test the return items of getCollations() fit 'type'
let a_to_z = "abcdefghijklmnopqrstuvwxyz";
let regex = /^[a-zA-Z0-9]{3,8}(-[a-zA-Z0-9]{3,8})*$/;
for (var i = 0; i < a_to_z.length; i++) {
  for (var j = 0; j < a_to_z.length; j++) {
    let l = a_to_z[i] + a_to_z[j];
    let locale = new Intl.Locale(l);
    print(locale);
    locale.getCollations().forEach(function(tokens) {
      assertTrue(regex.test(tokens),
          locale + ".collations [" + locale.getCollations() +
          "] does not meet 'type: alphanum{3,8}(sep alphanum{3,8})*'");
    });
    assertArrayEquals(locale.getCollations(), locale.getCollations().sort());
  }
}
