// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Check the return array are sorted
function checkSortedArray(l, name, items) {
  assertEquals([...items].sort(), items,
      "return value of " + l + "." + name + "should be sorted");
}
function checkLocale(locale) {
  let l = new Intl.Locale(locale)
  checkSortedArray(l, "timeZones", l.timeZones);
}

checkLocale("ar-EG");
checkLocale("fr-FR");
checkLocale("en-GB");
checkLocale("en-US");
checkLocale("en-AU");
checkLocale("en-CA");
checkLocale("zh-TW");
checkLocale("zh-CN");
checkLocale("ja-JP");
checkLocale("in-IN");
checkLocale("ru-RU");
