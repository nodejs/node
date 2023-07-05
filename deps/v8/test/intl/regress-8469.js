// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The following tz are NOT impacted by v8:8469
var some_tz_list = [
  "ciabj",
  "ghacc",
];

// The following tz ARE impacted by v8:8469
var problem_tz_list = [
  "etadd",
  "tzdar",
  "eheai",
  "sttms",
  "arirj",
  "arrgl",
  "aruaq",
  "arluq",
  "mxpvr",
  "brbvb",
  "arbue",
  "caycb",
  "brcgr",
  "cayzs",
  "crsjo",
  "caydq",
  "svsal",
  "cafne",
  "caglb",
  "cagoo",
  "tcgdt",
  "ustel",
  "bolpb",
  "uslax",
  "sxphi",
  "mxmex",
  "usnyc",
  "usxul",
  "usndcnt",
  "usndnsl",
  "ttpos",
  "brpvh",
  "prsju",
  "clpuq",
  "caffs",
  "cayek",
  "brrbr",
  "mxstis",
  "dosdq",
  "brsao",
  "gpsbh",
  "casjf",
  "knbas",
  "lccas",
  "vistt",
  "vcsvd",
  "cayyn",
  "cathu",
  "hkhkg",
  "mykul",
  "khpnh",
  "cvrai",
  "gsgrv",
  "shshn",
  "aubhq",
  "auldh",
  "imdgs",
  "smsai",
  "asppg",
  "pgpom",
];

let expectedTimeZone = (new Intl.DateTimeFormat("en"))
    .resolvedOptions().timeZone;

function testTz(tz) {
  print(tz);
  let timeZone = (new Intl.DateTimeFormat("en-u-tz-" + tz))
      .resolvedOptions().timeZone;
  assertEquals(expectedTimeZone, timeZone);
}

// first test soem tz not impacted by v8:8469 to ensure testTz is correct.
for (var tz of some_tz_list) testTz(tz);
for (var tz of problem_tz_list) testTz(tz);
