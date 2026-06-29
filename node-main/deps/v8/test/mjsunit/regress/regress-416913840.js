// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const str = '{"":13.3,"😊":[{"😊":0.011461940429979247},' +
  '{"😊":{"😊":{}}},{"":[]}],"":[{}],"*":[[]]}';
const expected = '{"":[{}],"😊":[{"😊":0.011461940429979247},' +
  '{"😊":{"😊":{}}},{"":[]}],"*":[[]]}';
assertEquals(expected, JSON.stringify(JSON.parse(str)));
