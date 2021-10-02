// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

// https://tc39.es/proposal-temporal/#sec-temporal.calendar.prototype.dateadd
let cal = new Temporal.Calendar("iso8601");

let p1y = new Temporal.Duration(1);
let p4y = new Temporal.Duration(4);
let p5m = new Temporal.Duration(0, 5);
let p1y2m = new Temporal.Duration(1,2);
let p1y4d = new Temporal.Duration(1,0,0,4);
let p1y2m4d = new Temporal.Duration(1,2,0,4);
let p10d = new Temporal.Duration(0,0,0,10);
let p1w = new Temporal.Duration(0,0,1);
let p6w = new Temporal.Duration(0,0,6);
let p2w3d = new Temporal.Duration(0,0,2,3);
let p1y2w = new Temporal.Duration(1,0,2);
let p2m3w = new Temporal.Duration(0,2,3);

assertEquals("2021-02-28", cal.dateAdd("2020-02-29", p1y).toJSON());
assertEquals("2024-02-29", cal.dateAdd("2020-02-29", p4y).toJSON());
assertEquals("2022-07-16", cal.dateAdd("2021-07-16", p1y).toJSON());

assertEquals("2021-12-16", cal.dateAdd("2021-07-16", p5m).toJSON());
assertEquals("2022-01-16", cal.dateAdd("2021-08-16", p5m).toJSON());
assertEquals("2022-03-31", cal.dateAdd("2021-10-31", p5m).toJSON());
assertEquals("2022-02-28", cal.dateAdd("2021-09-30", p5m).toJSON());
assertEquals("2020-02-29", cal.dateAdd("2019-09-30", p5m).toJSON());
assertEquals("2020-03-01", cal.dateAdd("2019-10-01", p5m).toJSON());

assertEquals("2022-09-16", cal.dateAdd("2021-07-16", p1y2m).toJSON());
assertEquals("2023-01-30", cal.dateAdd("2021-11-30", p1y2m).toJSON());
assertEquals("2023-02-28", cal.dateAdd("2021-12-31", p1y2m).toJSON());
assertEquals("2024-02-29", cal.dateAdd("2022-12-31", p1y2m).toJSON());

assertEquals("2022-07-20", cal.dateAdd("2021-07-16", p1y4d).toJSON());
assertEquals("2022-03-03", cal.dateAdd("2021-02-27", p1y4d).toJSON());
assertEquals("2024-03-02", cal.dateAdd("2023-02-27", p1y4d).toJSON());
assertEquals("2023-01-03", cal.dateAdd("2021-12-30", p1y4d).toJSON());
assertEquals("2022-08-03", cal.dateAdd("2021-07-30", p1y4d).toJSON());
assertEquals("2022-07-04", cal.dateAdd("2021-06-30", p1y4d).toJSON());

assertEquals("2022-09-20", cal.dateAdd("2021-07-16", p1y2m4d).toJSON());
assertEquals("2022-05-01", cal.dateAdd("2021-02-27", p1y2m4d).toJSON());
assertEquals("2022-04-30", cal.dateAdd("2021-02-26", p1y2m4d).toJSON());
assertEquals("2024-04-30", cal.dateAdd("2023-02-26", p1y2m4d).toJSON());
assertEquals("2023-03-04", cal.dateAdd("2021-12-30", p1y2m4d).toJSON());
assertEquals("2022-10-04", cal.dateAdd("2021-07-30", p1y2m4d).toJSON());
assertEquals("2022-09-03", cal.dateAdd("2021-06-30", p1y2m4d).toJSON());

assertEquals("2021-07-26", cal.dateAdd("2021-07-16", p10d).toJSON());
assertEquals("2021-08-05", cal.dateAdd("2021-07-26", p10d).toJSON());
assertEquals("2022-01-05", cal.dateAdd("2021-12-26", p10d).toJSON());
assertEquals("2020-03-07", cal.dateAdd("2020-02-26", p10d).toJSON());
assertEquals("2021-03-08", cal.dateAdd("2021-02-26", p10d).toJSON());
assertEquals("2020-02-29", cal.dateAdd("2020-02-19", p10d).toJSON());
assertEquals("2021-03-01", cal.dateAdd("2021-02-19", p10d).toJSON());

assertEquals("2021-02-26", cal.dateAdd("2021-02-19", p1w).toJSON());
assertEquals("2021-03-06", cal.dateAdd("2021-02-27", p1w).toJSON());
assertEquals("2020-03-05", cal.dateAdd("2020-02-27", p1w).toJSON());
assertEquals("2021-12-31", cal.dateAdd("2021-12-24", p1w).toJSON());
assertEquals("2022-01-03", cal.dateAdd("2021-12-27", p1w).toJSON());
assertEquals("2021-02-03", cal.dateAdd("2021-01-27", p1w).toJSON());
assertEquals("2021-07-04", cal.dateAdd("2021-06-27", p1w).toJSON());
assertEquals("2021-08-03", cal.dateAdd("2021-07-27", p1w).toJSON());

assertEquals("2021-04-02", cal.dateAdd("2021-02-19", p6w).toJSON());
assertEquals("2021-04-10", cal.dateAdd("2021-02-27", p6w).toJSON());
assertEquals("2020-04-09", cal.dateAdd("2020-02-27", p6w).toJSON());
assertEquals("2022-02-04", cal.dateAdd("2021-12-24", p6w).toJSON());
assertEquals("2022-02-07", cal.dateAdd("2021-12-27", p6w).toJSON());
assertEquals("2021-03-10", cal.dateAdd("2021-01-27", p6w).toJSON());
assertEquals("2021-08-08", cal.dateAdd("2021-06-27", p6w).toJSON());
assertEquals("2021-09-07", cal.dateAdd("2021-07-27", p6w).toJSON());

assertEquals("2020-03-17", cal.dateAdd("2020-02-29", p2w3d).toJSON());
assertEquals("2020-03-16", cal.dateAdd("2020-02-28", p2w3d).toJSON());
assertEquals("2021-03-17", cal.dateAdd("2021-02-28", p2w3d).toJSON());
assertEquals("2021-01-14", cal.dateAdd("2020-12-28", p2w3d).toJSON());

assertEquals("2021-03-14", cal.dateAdd("2020-02-29", p1y2w).toJSON());
assertEquals("2021-03-14", cal.dateAdd("2020-02-28", p1y2w).toJSON());
assertEquals("2022-03-14", cal.dateAdd("2021-02-28", p1y2w).toJSON());
assertEquals("2022-01-11", cal.dateAdd("2020-12-28", p1y2w).toJSON());

assertEquals("2020-05-20", cal.dateAdd("2020-02-29", p2m3w).toJSON());
assertEquals("2020-05-19", cal.dateAdd("2020-02-28", p2m3w).toJSON());
assertEquals("2021-05-19", cal.dateAdd("2021-02-28", p2m3w).toJSON());
assertEquals("2021-03-21", cal.dateAdd("2020-12-28", p2m3w).toJSON());
assertEquals("2020-03-20", cal.dateAdd("2019-12-28", p2m3w).toJSON());
assertEquals("2020-01-18", cal.dateAdd("2019-10-28", p2m3w).toJSON());
assertEquals("2020-01-21", cal.dateAdd("2019-10-31", p2m3w).toJSON());
