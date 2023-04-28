// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Check the return array has no undefined
function checkNoUndefined(l, items) {
  items.forEach( function(item) {
    assertNotUndefined(item, l + ": [" +items + "] should not have undefined");
  });
}
function checkLocale(locale) {
  let l = new Intl.Locale(locale)
  checkNoUndefined(l, l.calendars);
  checkNoUndefined(l, l.collations);
  checkNoUndefined(l, l.hourCycles);
  checkNoUndefined(l, l.numberingSystems);
  if (l.region != undefined) {
    checkNoUndefined(l, l.timeZones);
  }
}

checkLocale("ar");
checkLocale("en");
checkLocale("fr");
checkLocale("en-GB");
checkLocale("en-US");
checkLocale("zh-TW");
checkLocale("zh-CN");
