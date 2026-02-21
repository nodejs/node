// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function timezone(tz) {
  var str = (new Date(2014, 0, 10)).toString();
  if (tz == "CET") {
    return str == "Fri Jan 10 2014 00:00:00 GMT+0100 (CET)";
  }
  if (tz == "BRT") {
    return str == "Fri Jan 10 2014 00:00:00 GMT-0200 (BRST)";
  }
  if (tz == "PST") {
    return str == "Fri Jan 10 2014 00:00:00 GMT-0800 (PST)";
  }
  return false;
}

if (timezone("CET")) {
  assertEquals("Sat Mar 29 2014 22:59:00 GMT+0100 (CET)",
               (new Date(2014, 2, 29, 22, 59)).toString());
  assertEquals("Sat, 29 Mar 2014 21:59:00 GMT",
               (new Date(2014, 2, 29, 22, 59)).toUTCString());
  assertEquals("Sat Mar 29 2014 23:00:00 GMT+0100 (CET)",
               (new Date(2014, 2, 29, 23, 0)).toString());
  assertEquals("Sat, 29 Mar 2014 22:00:00 GMT",
               (new Date(2014, 2, 29, 23, 0)).toUTCString());
  assertEquals("Sat Mar 29 2014 23:59:00 GMT+0100 (CET)",
               (new Date(2014, 2, 29, 23, 59)).toString());
  assertEquals("Sat, 29 Mar 2014 22:59:00 GMT",
               (new Date(2014, 2, 29, 23, 59)).toUTCString());
  assertEquals("Sun Mar 30 2014 00:00:00 GMT+0100 (CET)",
               (new Date(2014, 2, 30, 0, 0)).toString());
  assertEquals("Sat, 29 Mar 2014 23:00:00 GMT",
               (new Date(2014, 2, 30, 0, 0)).toUTCString());
  assertEquals("Sun Mar 30 2014 00:59:00 GMT+0100 (CET)",
               (new Date(2014, 2, 30, 0, 59)).toString());
  assertEquals("Sat, 29 Mar 2014 23:59:00 GMT",
               (new Date(2014, 2, 30, 0, 59)).toUTCString());
  assertEquals("Sun Mar 30 2014 01:00:00 GMT+0100 (CET)",
               (new Date(2014, 2, 30, 1, 0)).toString());
  assertEquals("Sun, 30 Mar 2014 00:00:00 GMT",
               (new Date(2014, 2, 30, 1, 0)).toUTCString());
  assertEquals("Sun Mar 30 2014 01:59:00 GMT+0100 (CET)",
               (new Date(2014, 2, 30, 1, 59)).toString());
  assertEquals("Sun, 30 Mar 2014 00:59:00 GMT",
               (new Date(2014, 2, 30, 1, 59)).toUTCString());
  assertEquals("Sun Mar 30 2014 03:00:00 GMT+0200 (CEST)",
               (new Date(2014, 2, 30, 2, 0)).toString());
  assertEquals("Sun, 30 Mar 2014 01:00:00 GMT",
               (new Date(2014, 2, 30, 2, 0)).toUTCString());
  assertEquals("Sun Mar 30 2014 03:59:00 GMT+0200 (CEST)",
               (new Date(2014, 2, 30, 2, 59)).toString());
  assertEquals("Sun, 30 Mar 2014 01:59:00 GMT",
               (new Date(2014, 2, 30, 2, 59)).toUTCString());
  assertEquals("Sun Mar 30 2014 03:00:00 GMT+0200 (CEST)",
               (new Date(2014, 2, 30, 3, 0)).toString());
  assertEquals("Sun, 30 Mar 2014 01:00:00 GMT",
               (new Date(2014, 2, 30, 3, 0)).toUTCString());
  assertEquals("Sun Mar 30 2014 03:59:00 GMT+0200 (CEST)",
               (new Date(2014, 2, 30, 3, 59)).toString());
  assertEquals("Sun, 30 Mar 2014 01:59:00 GMT",
               (new Date(2014, 2, 30, 3, 59)).toUTCString());
  assertEquals("Sun Mar 30 2014 04:00:00 GMT+0200 (CEST)",
               (new Date(2014, 2, 30, 4, 0)).toString());
  assertEquals("Sun, 30 Mar 2014 02:00:00 GMT",
               (new Date(2014, 2, 30, 4, 0)).toUTCString());
  assertEquals("Sat Oct 25 2014 22:59:00 GMT+0200 (CEST)",
               (new Date(2014, 9, 25, 22, 59)).toString());
  assertEquals("Sat, 25 Oct 2014 20:59:00 GMT",
               (new Date(2014, 9, 25, 22, 59)).toUTCString());
  assertEquals("Sat Oct 25 2014 23:00:00 GMT+0200 (CEST)",
               (new Date(2014, 9, 25, 23, 0)).toString());
  assertEquals("Sat, 25 Oct 2014 21:00:00 GMT",
               (new Date(2014, 9, 25, 23, 0)).toUTCString());
  assertEquals("Sat Oct 25 2014 23:59:00 GMT+0200 (CEST)",
               (new Date(2014, 9, 25, 23, 59)).toString());
  assertEquals("Sat, 25 Oct 2014 21:59:00 GMT",
               (new Date(2014, 9, 25, 23, 59)).toUTCString());
  assertEquals("Sun Oct 26 2014 00:00:00 GMT+0200 (CEST)",
               (new Date(2014, 9, 26, 0, 0)).toString());
  assertEquals("Sat, 25 Oct 2014 22:00:00 GMT",
               (new Date(2014, 9, 26, 0, 0)).toUTCString());
  assertEquals("Sun Oct 26 2014 00:59:00 GMT+0200 (CEST)",
               (new Date(2014, 9, 26, 0, 59)).toString());
  assertEquals("Sat, 25 Oct 2014 22:59:00 GMT",
               (new Date(2014, 9, 26, 0, 59)).toUTCString());
  assertEquals("Sun Oct 26 2014 01:00:00 GMT+0200 (CEST)",
               (new Date(2014, 9, 26, 1, 0)).toString());
  assertEquals("Sat, 25 Oct 2014 23:00:00 GMT",
               (new Date(2014, 9, 26, 1, 0)).toUTCString());
  assertEquals("Sun Oct 26 2014 01:59:00 GMT+0200 (CEST)",
               (new Date(2014, 9, 26, 1, 59)).toString());
  assertEquals("Sat, 25 Oct 2014 23:59:00 GMT",
               (new Date(2014, 9, 26, 1, 59)).toUTCString());
  assertEquals("Sun Oct 26 2014 02:00:00 GMT+0200 (CEST)",
               (new Date(2014, 9, 26, 2, 0)).toString());
  assertEquals("Sun, 26 Oct 2014 00:00:00 GMT",
               (new Date(2014, 9, 26, 2, 0)).toUTCString());
  assertEquals("Sun Oct 26 2014 02:59:00 GMT+0200 (CEST)",
               (new Date(2014, 9, 26, 2, 59)).toString());
  assertEquals("Sun, 26 Oct 2014 00:59:00 GMT",
               (new Date(2014, 9, 26, 2, 59)).toUTCString());
  assertEquals("Sun Oct 26 2014 03:00:00 GMT+0100 (CET)",
               (new Date(2014, 9, 26, 3, 0)).toString());
  assertEquals("Sun, 26 Oct 2014 02:00:00 GMT",
               (new Date(2014, 9, 26, 3, 0)).toUTCString());
  assertEquals("Sun Oct 26 2014 03:59:00 GMT+0100 (CET)",
               (new Date(2014, 9, 26, 3, 59)).toString());
  assertEquals("Sun, 26 Oct 2014 02:59:00 GMT",
               (new Date(2014, 9, 26, 3, 59)).toUTCString());
  assertEquals("Sun Oct 26 2014 04:00:00 GMT+0100 (CET)",
               (new Date(2014, 9, 26, 4, 0)).toString());
  assertEquals("Sun, 26 Oct 2014 03:00:00 GMT",
               (new Date(2014, 9, 26, 4, 0)).toUTCString());
}

if (timezone("BRT")) {
  assertEquals("Sat Oct 18 2014 22:59:00 GMT-0300 (BRT)",
               (new Date(2014, 9, 18, 22, 59)).toString());
  assertEquals("Sun, 19 Oct 2014 01:59:00 GMT",
               (new Date(2014, 9, 18, 22, 59)).toUTCString());
  assertEquals("Sat Oct 18 2014 23:00:00 GMT-0300 (BRT)",
               (new Date(2014, 9, 18, 23, 0)).toString());
  assertEquals("Sun, 19 Oct 2014 02:00:00 GMT",
               (new Date(2014, 9, 18, 23, 0)).toUTCString());
  assertEquals("Sat Oct 18 2014 23:59:00 GMT-0300 (BRT)",
               (new Date(2014, 9, 18, 23, 59)).toString());
  assertEquals("Sun, 19 Oct 2014 02:59:00 GMT",
               (new Date(2014, 9, 18, 23, 59)).toUTCString());
  assertEquals("Sun Oct 19 2014 01:00:00 GMT-0200 (BRST)",
               (new Date(2014, 9, 19, 0, 0)).toString());
  assertEquals("Sun, 19 Oct 2014 03:00:00 GMT",
               (new Date(2014, 9, 19, 0, 0)).toUTCString());
  assertEquals("Sun Oct 19 2014 01:59:00 GMT-0200 (BRST)",
               (new Date(2014, 9, 19, 0, 59)).toString());
  assertEquals("Sun, 19 Oct 2014 03:59:00 GMT",
               (new Date(2014, 9, 19, 0, 59)).toUTCString());
  assertEquals("Sun Oct 19 2014 01:00:00 GMT-0200 (BRST)",
               (new Date(2014, 9, 19, 1, 0)).toString());
  assertEquals("Sun, 19 Oct 2014 03:00:00 GMT",
               (new Date(2014, 9, 19, 1, 0)).toUTCString());
  assertEquals("Sun Oct 19 2014 01:59:00 GMT-0200 (BRST)",
               (new Date(2014, 9, 19, 1, 59)).toString());
  assertEquals("Sun, 19 Oct 2014 03:59:00 GMT",
               (new Date(2014, 9, 19, 1, 59)).toUTCString());
  assertEquals("Sun Oct 19 2014 02:00:00 GMT-0200 (BRST)",
               (new Date(2014, 9, 19, 2, 0)).toString());
  assertEquals("Sun, 19 Oct 2014 04:00:00 GMT",
               (new Date(2014, 9, 19, 2, 0)).toUTCString());
  assertEquals("Sun Oct 19 2014 02:59:00 GMT-0200 (BRST)",
               (new Date(2014, 9, 19, 2, 59)).toString());
  assertEquals("Sun, 19 Oct 2014 04:59:00 GMT",
               (new Date(2014, 9, 19, 2, 59)).toUTCString());
  assertEquals("Sun Oct 19 2014 03:00:00 GMT-0200 (BRST)",
               (new Date(2014, 9, 19, 3, 0)).toString());
  assertEquals("Sun, 19 Oct 2014 05:00:00 GMT",
               (new Date(2014, 9, 19, 3, 0)).toUTCString());
  assertEquals("Sun Oct 19 2014 03:59:00 GMT-0200 (BRST)",
               (new Date(2014, 9, 19, 3, 59)).toString());
  assertEquals("Sun, 19 Oct 2014 05:59:00 GMT",
               (new Date(2014, 9, 19, 3, 59)).toUTCString());
  assertEquals("Sun Oct 19 2014 04:00:00 GMT-0200 (BRST)",
               (new Date(2014, 9, 19, 4, 0)).toString());
  assertEquals("Sun, 19 Oct 2014 06:00:00 GMT",
               (new Date(2014, 9, 19, 4, 0)).toUTCString());
  assertEquals("Sat Feb 15 2014 22:59:00 GMT-0200 (BRST)",
               (new Date(2014, 1, 15, 22, 59)).toString());
  assertEquals("Sun, 16 Feb 2014 00:59:00 GMT",
               (new Date(2014, 1, 15, 22, 59)).toUTCString());
  assertEquals("Sat Feb 15 2014 23:00:00 GMT-0200 (BRST)",
               (new Date(2014, 1, 15, 23, 0)).toString());
  assertEquals("Sun, 16 Feb 2014 01:00:00 GMT",
               (new Date(2014, 1, 15, 23, 0)).toUTCString());
  assertEquals("Sat Feb 15 2014 23:59:00 GMT-0200 (BRST)",
               (new Date(2014, 1, 15, 23, 59)).toString());
  assertEquals("Sun, 16 Feb 2014 01:59:00 GMT",
               (new Date(2014, 1, 15, 23, 59)).toUTCString());
  assertEquals("Sun Feb 16 2014 00:00:00 GMT-0300 (BRT)",
               (new Date(2014, 1, 16, 0, 0)).toString());
  assertEquals("Sun, 16 Feb 2014 03:00:00 GMT",
               (new Date(2014, 1, 16, 0, 0)).toUTCString());
  assertEquals("Sun Feb 16 2014 00:59:00 GMT-0300 (BRT)",
               (new Date(2014, 1, 16, 0, 59)).toString());
  assertEquals("Sun, 16 Feb 2014 03:59:00 GMT",
               (new Date(2014, 1, 16, 0, 59)).toUTCString());
  assertEquals("Sun Feb 16 2014 01:00:00 GMT-0300 (BRT)",
               (new Date(2014, 1, 16, 1, 0)).toString());
  assertEquals("Sun, 16 Feb 2014 04:00:00 GMT",
               (new Date(2014, 1, 16, 1, 0)).toUTCString());
  assertEquals("Sun Feb 16 2014 01:59:00 GMT-0300 (BRT)",
               (new Date(2014, 1, 16, 1, 59)).toString());
  assertEquals("Sun, 16 Feb 2014 04:59:00 GMT",
               (new Date(2014, 1, 16, 1, 59)).toUTCString());
  assertEquals("Sun Feb 16 2014 02:00:00 GMT-0300 (BRT)",
               (new Date(2014, 1, 16, 2, 0)).toString());
  assertEquals("Sun, 16 Feb 2014 05:00:00 GMT",
               (new Date(2014, 1, 16, 2, 0)).toUTCString());
  assertEquals("Sun Feb 16 2014 02:59:00 GMT-0300 (BRT)",
               (new Date(2014, 1, 16, 2, 59)).toString());
  assertEquals("Sun, 16 Feb 2014 05:59:00 GMT",
               (new Date(2014, 1, 16, 2, 59)).toUTCString());
  assertEquals("Sun Feb 16 2014 03:00:00 GMT-0300 (BRT)",
               (new Date(2014, 1, 16, 3, 0)).toString());
  assertEquals("Sun, 16 Feb 2014 06:00:00 GMT",
               (new Date(2014, 1, 16, 3, 0)).toUTCString());
  assertEquals("Sun Feb 16 2014 03:59:00 GMT-0300 (BRT)",
               (new Date(2014, 1, 16, 3, 59)).toString());
  assertEquals("Sun, 16 Feb 2014 06:59:00 GMT",
               (new Date(2014, 1, 16, 3, 59)).toUTCString());
  assertEquals("Sun Feb 16 2014 04:00:00 GMT-0300 (BRT)",
               (new Date(2014, 1, 16, 4, 0)).toString());
  assertEquals("Sun, 16 Feb 2014 07:00:00 GMT",
               (new Date(2014, 1, 16, 4, 0)).toUTCString());
}

if (timezone("PST")) {
  assertEquals("Sat Mar 08 2014 22:59:00 GMT-0800 (PST)",
               (new Date(2014, 2, 8, 22, 59)).toString());
  assertEquals("Sun, 09 Mar 2014 06:59:00 GMT",
               (new Date(2014, 2, 8, 22, 59)).toUTCString());
  assertEquals("Sat Mar 08 2014 23:00:00 GMT-0800 (PST)",
               (new Date(2014, 2, 8, 23, 0)).toString());
  assertEquals("Sun, 09 Mar 2014 07:00:00 GMT",
               (new Date(2014, 2, 8, 23, 0)).toUTCString());
  assertEquals("Sat Mar 08 2014 23:59:00 GMT-0800 (PST)",
               (new Date(2014, 2, 8, 23, 59)).toString());
  assertEquals("Sun, 09 Mar 2014 07:59:00 GMT",
               (new Date(2014, 2, 8, 23, 59)).toUTCString());
  assertEquals("Sun Mar 09 2014 00:00:00 GMT-0800 (PST)",
               (new Date(2014, 2, 9, 0, 0)).toString());
  assertEquals("Sun, 09 Mar 2014 08:00:00 GMT",
               (new Date(2014, 2, 9, 0, 0)).toUTCString());
  assertEquals("Sun Mar 09 2014 00:59:00 GMT-0800 (PST)",
               (new Date(2014, 2, 9, 0, 59)).toString());
  assertEquals("Sun, 09 Mar 2014 08:59:00 GMT",
               (new Date(2014, 2, 9, 0, 59)).toUTCString());
  assertEquals("Sun Mar 09 2014 01:00:00 GMT-0800 (PST)",
               (new Date(2014, 2, 9, 1, 0)).toString());
  assertEquals("Sun, 09 Mar 2014 09:00:00 GMT",
               (new Date(2014, 2, 9, 1, 0)).toUTCString());
  assertEquals("Sun Mar 09 2014 01:59:00 GMT-0800 (PST)",
               (new Date(2014, 2, 9, 1, 59)).toString());
  assertEquals("Sun, 09 Mar 2014 09:59:00 GMT",
               (new Date(2014, 2, 9, 1, 59)).toUTCString());
  assertEquals("Sun Mar 09 2014 03:00:00 GMT-0700 (PDT)",
               (new Date(2014, 2, 9, 2, 0)).toString());
  assertEquals("Sun, 09 Mar 2014 10:00:00 GMT",
               (new Date(2014, 2, 9, 2, 0)).toUTCString());
  assertEquals("Sun Mar 09 2014 03:59:00 GMT-0700 (PDT)",
               (new Date(2014, 2, 9, 2, 59)).toString());
  assertEquals("Sun, 09 Mar 2014 10:59:00 GMT",
               (new Date(2014, 2, 9, 2, 59)).toUTCString());
  assertEquals("Sun Mar 09 2014 03:00:00 GMT-0700 (PDT)",
               (new Date(2014, 2, 9, 3, 0)).toString());
  assertEquals("Sun, 09 Mar 2014 10:00:00 GMT",
               (new Date(2014, 2, 9, 3, 0)).toUTCString());
  assertEquals("Sun Mar 09 2014 03:59:00 GMT-0700 (PDT)",
               (new Date(2014, 2, 9, 3, 59)).toString());
  assertEquals("Sun, 09 Mar 2014 10:59:00 GMT",
               (new Date(2014, 2, 9, 3, 59)).toUTCString());
  assertEquals("Sun Mar 09 2014 04:00:00 GMT-0700 (PDT)",
               (new Date(2014, 2, 9, 4, 0)).toString());
  assertEquals("Sun, 09 Mar 2014 11:00:00 GMT",
               (new Date(2014, 2, 9, 4, 0)).toUTCString());
  assertEquals("Sat Nov 01 2014 22:59:00 GMT-0700 (PDT)",
               (new Date(2014, 10, 1, 22, 59)).toString());
  assertEquals("Sun, 02 Nov 2014 05:59:00 GMT",
               (new Date(2014, 10, 1, 22, 59)).toUTCString());
  assertEquals("Sat Nov 01 2014 23:00:00 GMT-0700 (PDT)",
               (new Date(2014, 10, 1, 23, 0)).toString());
  assertEquals("Sun, 02 Nov 2014 06:00:00 GMT",
               (new Date(2014, 10, 1, 23, 0)).toUTCString());
  assertEquals("Sat Nov 01 2014 23:59:00 GMT-0700 (PDT)",
               (new Date(2014, 10, 1, 23, 59)).toString());
  assertEquals("Sun, 02 Nov 2014 06:59:00 GMT",
               (new Date(2014, 10, 1, 23, 59)).toUTCString());
  assertEquals("Sun Nov 02 2014 00:00:00 GMT-0700 (PDT)",
               (new Date(2014, 10, 2, 0, 0)).toString());
  assertEquals("Sun, 02 Nov 2014 07:00:00 GMT",
               (new Date(2014, 10, 2, 0, 0)).toUTCString());
  assertEquals("Sun Nov 02 2014 00:59:00 GMT-0700 (PDT)",
               (new Date(2014, 10, 2, 0, 59)).toString());
  assertEquals("Sun, 02 Nov 2014 07:59:00 GMT",
               (new Date(2014, 10, 2, 0, 59)).toUTCString());
  assertEquals("Sun Nov 02 2014 01:00:00 GMT-0700 (PDT)",
               (new Date(2014, 10, 2, 1, 0)).toString());
  assertEquals("Sun, 02 Nov 2014 08:00:00 GMT",
               (new Date(2014, 10, 2, 1, 0)).toUTCString());
  assertEquals("Sun Nov 02 2014 01:59:00 GMT-0700 (PDT)",
               (new Date(2014, 10, 2, 1, 59)).toString());
  assertEquals("Sun, 02 Nov 2014 08:59:00 GMT",
               (new Date(2014, 10, 2, 1, 59)).toUTCString());
  assertEquals("Sun Nov 02 2014 02:00:00 GMT-0800 (PST)",
               (new Date(2014, 10, 2, 2, 0)).toString());
  assertEquals("Sun, 02 Nov 2014 10:00:00 GMT",
               (new Date(2014, 10, 2, 2, 0)).toUTCString());
  assertEquals("Sun Nov 02 2014 02:59:00 GMT-0800 (PST)",
               (new Date(2014, 10, 2, 2, 59)).toString());
  assertEquals("Sun, 02 Nov 2014 10:59:00 GMT",
               (new Date(2014, 10, 2, 2, 59)).toUTCString());
  assertEquals("Sun Nov 02 2014 03:00:00 GMT-0800 (PST)",
               (new Date(2014, 10, 2, 3, 0)).toString());
  assertEquals("Sun, 02 Nov 2014 11:00:00 GMT",
               (new Date(2014, 10, 2, 3, 0)).toUTCString());
  assertEquals("Sun Nov 02 2014 03:59:00 GMT-0800 (PST)",
               (new Date(2014, 10, 2, 3, 59)).toString());
  assertEquals("Sun, 02 Nov 2014 11:59:00 GMT",
               (new Date(2014, 10, 2, 3, 59)).toUTCString());
  assertEquals("Sun Nov 02 2014 04:00:00 GMT-0800 (PST)",
               (new Date(2014, 10, 2, 4, 0)).toString());
  assertEquals("Sun, 02 Nov 2014 12:00:00 GMT",
               (new Date(2014, 10, 2, 4, 0)).toUTCString());
}
