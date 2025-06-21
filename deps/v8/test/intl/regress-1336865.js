// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let nf = new Intl.NumberFormat();
let actual1 = nf.formatRangeToParts(1, 1);
/*
[{type: "approximatelySign", value: "~", source: "shared"},
 {type: "integer", value: "1", source: "shared"}]
 */
assertEquals(2, actual1.length);
assertEquals("approximatelySign", actual1[0].type);
assertEquals("~", actual1[0].value);
assertEquals("shared", actual1[0].source);
assertEquals("integer", actual1[1].type);
assertEquals("1", actual1[1].value);
assertEquals("shared", actual1[1].source);
/*
[{type: "approximatelySign", value: "~", source: "shared"},
 {type: "integer", value: "9", source: "shared"},
 {type: "group", value: ",", source: "shared"},
 {type: "integer", value: "223", source: "shared"},
 {type: "group", value: ",", source: "shared"},
 {type: "integer", value: "372", source: "shared"},
 {type: "group", value: ",", source: "shared"},
 {type: "integer", value: "036", source: "shared"},
 {type: "group", value: ",", source: "shared"},
 {type: "integer", value: "854", source: "shared"},
 {type: "group", value: ",", source: "shared"},
 {type: "integer", value: "775", source: "shared"},
 {type: "group", value: ",, source: "shared""},
 {type: "integer", value: "807", source: "shared"}]
*/
let bigint =  12345678901234567890n;
let actual2 = nf.formatRangeToParts(bigint, bigint);
assertEquals(14, actual2.length);
assertEquals("approximatelySign", actual2[0].type);
assertEquals("~", actual2[0].value);
assertEquals("shared", actual2[0].source);
assertEquals("integer", actual2[1].type);
assertEquals("12", actual2[1].value);
assertEquals("shared", actual2[1].source);
assertEquals("group", actual2[2].type);
assertEquals(",", actual2[2].value);
assertEquals("shared", actual2[2].source);
assertEquals("integer", actual2[3].type);
assertEquals("345", actual2[3].value);
assertEquals("shared", actual2[3].source);
assertEquals("group", actual2[4].type);
assertEquals(",", actual2[4].value);
assertEquals("shared", actual2[4].source);
assertEquals("integer", actual2[5].type);
assertEquals("678", actual2[5].value);
assertEquals("shared", actual2[5].source);
assertEquals("group", actual2[6].type);
assertEquals(",", actual2[6].value);
assertEquals("shared", actual2[6].source);
assertEquals("integer", actual2[7].type);
assertEquals("901", actual2[7].value);
assertEquals("shared", actual2[7].source);
assertEquals("group", actual2[8].type);
assertEquals(",", actual2[8].value);
assertEquals("shared", actual2[8].source);
assertEquals("integer", actual2[9].type);
assertEquals("234", actual2[9].value);
assertEquals("shared", actual2[9].source);
assertEquals("group", actual2[10].type);
assertEquals(",", actual2[10].value);
assertEquals("shared", actual2[10].source);
assertEquals("integer", actual2[11].type);
assertEquals("567", actual2[11].value);
assertEquals("shared", actual2[11].source);
assertEquals("group", actual2[12].type);
assertEquals(",", actual2[12].value);
assertEquals("shared", actual2[12].source);
assertEquals("integer", actual2[13].type);
assertEquals("890", actual2[13].value);
assertEquals("shared", actual2[13].source);
