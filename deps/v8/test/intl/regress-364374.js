// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const special_time_zones = [
  "America/Argentina/ComodRivadavia",
  "America/Knox_IN",
  "Antarctica/McMurdo",
  "Australia/ACT",
  "Australia/LHI",
  "Australia/NSW",
  "Brazil/DeNoronha",
  "CET",
  "CST6CDT",
  "Chile/EasterIsland",
  "Etc/UCT",
  "EET",
  "EST",
  "EST5EDT",
  "GB",
  "GB-Eire",
  "GMT+0",
  "GMT-0",
  "GMT0",
  "HST",
  "MET",
  "MST",
  "MST7MDT",
  "Mexico/BajaNorte",
  "Mexico/BajaSur",
  "NZ",
  "NZ-CHAT",
  "PRC",
  "PST8PDT",
  "ROC",
  "ROK",
  "UCT",
  "US/Alaska",
  "US/Aleutian",
  "US/Arizona",
  "US/Central",
  "US/East-Indiana",
  "US/Eastern",
  "US/Hawaii",
  "US/Indiana-Starke",
  "US/Michigan",
  "US/Mountain",
  "US/Pacific",
  "US/Pacific-New",
  "US/Samoa",
  "W-SU",
  "WET",
];

special_time_zones.forEach(function(timeZone) {
  assertDoesNotThrow(() => {
    // Make sure the following wont throw RangeError exception
    df = new Intl.DateTimeFormat(undefined, {timeZone});
  });
})
