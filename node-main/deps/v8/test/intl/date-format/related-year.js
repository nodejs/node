// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test it will output relatedYear and yearName

let df = new Intl.DateTimeFormat("zh-u-ca-chinese", {year: "numeric"})
let date = new Date(2019, 5, 1);
assertEquals("2019己亥年", df.format(date));
assertEquals([{type: "relatedYear", value: "2019"},
              {type: "yearName", value: "己亥"},
              {type: "literal", value: "年"}],
             df.formatToParts(date));
