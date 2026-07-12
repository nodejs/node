// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

for (const invalidLocale of ["", "1", "12", "123", "1234"]) {
  print(invalidLocale);
  assertThrows(() => "".toLocaleUpperCase(invalidLocale), RangeError);
  assertThrows(() => "".toLocaleLowerCase(invalidLocale), RangeError);
}
