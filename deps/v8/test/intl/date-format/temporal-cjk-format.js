// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const kOptions = {
  year: "numeric",
  month: "long",
  day: "numeric",
  timeZone: "UTC"
};
const kDate = new Date(Date.UTC(2026, 7, 27, 12, 0, 0));
const kInstant = Temporal.Instant.from("2026-08-27T12:00:00Z");
const kPlainDate = Temporal.PlainDate.from("2026-08-27");

function assertTemporalMatchesDate(locale, options) {
  const formatter = new Intl.DateTimeFormat(locale, options);
  const expected = formatter.format(kDate);
  assertEquals(expected, formatter.format(kInstant));
  assertEquals(expected, formatter.format(kPlainDate));
}

assertTemporalMatchesDate("ja-JP", kOptions);
assertTemporalMatchesDate("zh-CN", kOptions);
assertTemporalMatchesDate("zh-TW", kOptions);
assertTemporalMatchesDate("ko-KR", kOptions);
assertTemporalMatchesDate("en-US", kOptions);

assertTemporalMatchesDate("ja-JP", {dateStyle: "long", timeZone: "UTC"});
assertTemporalMatchesDate("zh-CN", {dateStyle: "long", timeZone: "UTC"});
assertTemporalMatchesDate("ja-JP", {dateStyle: "full", timeZone: "UTC"});
