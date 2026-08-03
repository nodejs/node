// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

let d = new Temporal.Instant(0n);
assertEquals("1970-01-01T00:00:00Z", d.toJSON());
d = new Temporal.Instant(1n);
assertEquals("1970-01-01T00:00:00.000000001Z", d.toJSON());
d = new Temporal.Instant(12n);
assertEquals("1970-01-01T00:00:00.000000012Z", d.toJSON());
d = new Temporal.Instant(123n);
assertEquals("1970-01-01T00:00:00.000000123Z", d.toJSON());
d = new Temporal.Instant(1234n);
assertEquals("1970-01-01T00:00:00.000001234Z", d.toJSON());
d = new Temporal.Instant(12345n);
assertEquals("1970-01-01T00:00:00.000012345Z", d.toJSON());
d = new Temporal.Instant(123456n);
assertEquals("1970-01-01T00:00:00.000123456Z", d.toJSON());
d = new Temporal.Instant(1234567n);
assertEquals("1970-01-01T00:00:00.001234567Z", d.toJSON());
d = new Temporal.Instant(12345678n);
assertEquals("1970-01-01T00:00:00.012345678Z", d.toJSON());
d = new Temporal.Instant(123456789n);
assertEquals("1970-01-01T00:00:00.123456789Z", d.toJSON());
d = new Temporal.Instant(1234567891n);
assertEquals("1970-01-01T00:00:01.234567891Z", d.toJSON());
d = new Temporal.Instant(12345678912n);
assertEquals("1970-01-01T00:00:12.345678912Z", d.toJSON());

d = new Temporal.Instant(-1n);
assertEquals("1969-12-31T23:59:59.999999999Z", d.toJSON());
d = new Temporal.Instant(-12n);
assertEquals("1969-12-31T23:59:59.999999988Z", d.toJSON());
d = new Temporal.Instant(-123n);
assertEquals("1969-12-31T23:59:59.999999877Z", d.toJSON());
d = new Temporal.Instant(-1234n);
assertEquals("1969-12-31T23:59:59.999998766Z", d.toJSON());
d = new Temporal.Instant(-12345n);
assertEquals("1969-12-31T23:59:59.999987655Z", d.toJSON());
d = new Temporal.Instant(-123456n);
assertEquals("1969-12-31T23:59:59.999876544Z", d.toJSON());
d = new Temporal.Instant(-1234567n);
assertEquals("1969-12-31T23:59:59.998765433Z", d.toJSON());
d = new Temporal.Instant(-12345678n);
assertEquals("1969-12-31T23:59:59.987654322Z", d.toJSON());
d = new Temporal.Instant(-123456789n);
assertEquals("1969-12-31T23:59:59.876543211Z", d.toJSON());
d = new Temporal.Instant(-1234567891n);
assertEquals("1969-12-31T23:59:58.765432109Z", d.toJSON());
d = new Temporal.Instant(-12345678912n);
assertEquals("1969-12-31T23:59:47.654321088Z", d.toJSON());
