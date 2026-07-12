// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function runNearStackLimit(f) {
  function t() {
    try {
     return t();
    } catch (e) {
      return f();
    }
  }
  try {
    return t();
  } catch (e) {}
}

const str = 'hello';
const locale = new Intl.Locale('ja-u-co-eor-kf-lower-kn-false');

function test(getLocaleFromCollator) {
  const localeInCollator = getLocaleFromCollator(locale);
  const temp = str("en");
}

runNearStackLimit(() => {
    return test(args => {
      new Intl.Collator(locale).resolvedOptions().locale;
    })}
);
