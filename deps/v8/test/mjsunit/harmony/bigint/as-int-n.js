// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// BigInt.asIntN
{
  assertEquals(2, BigInt.asIntN.length);
}{
  assertEquals(-1n, BigInt.asIntN(3, 15n));
  assertEquals(-2n, BigInt.asIntN(3, 14n));
  assertEquals(-3n, BigInt.asIntN(3, 13n));
  assertEquals(-4n, BigInt.asIntN(3, 12n));
  assertEquals(3n, BigInt.asIntN(3, 11n));
  assertEquals(2n, BigInt.asIntN(3, 10n));
  assertEquals(1n, BigInt.asIntN(3, 9n));
  assertEquals(0n, BigInt.asIntN(3, 8n));
  assertEquals(-1n, BigInt.asIntN(3, 7n));
  assertEquals(-2n, BigInt.asIntN(3, 6n));
  assertEquals(-3n, BigInt.asIntN(3, 5n));
  assertEquals(-4n, BigInt.asIntN(3, 4n));
  assertEquals(3n, BigInt.asIntN(3, 3n));
  assertEquals(2n, BigInt.asIntN(3, 2n));
  assertEquals(1n, BigInt.asIntN(3, 1n));
  assertEquals(0n, BigInt.asIntN(3, 0n));
  assertEquals(-1n, BigInt.asIntN(3, -1n));
  assertEquals(-2n, BigInt.asIntN(3, -2n));
  assertEquals(-3n, BigInt.asIntN(3, -3n));
  assertEquals(-4n, BigInt.asIntN(3, -4n));
  assertEquals(3n, BigInt.asIntN(3, -5n));
  assertEquals(2n, BigInt.asIntN(3, -6n));
  assertEquals(1n, BigInt.asIntN(3, -7n));
  assertEquals(0n, BigInt.asIntN(3, -8n));
  assertEquals(-1n, BigInt.asIntN(3, -9n));
  assertEquals(-2n, BigInt.asIntN(3, -10n));
  assertEquals(-3n, BigInt.asIntN(3, -11n));
  assertEquals(-4n, BigInt.asIntN(3, -12n));
  assertEquals(3n, BigInt.asIntN(3, -13n));
  assertEquals(2n, BigInt.asIntN(3, -14n));
  assertEquals(1n, BigInt.asIntN(3, -15n));
}{
  assertEquals(254n, BigInt.asIntN(10, 254n));
  assertEquals(255n, BigInt.asIntN(10, 255n));
  assertEquals(256n, BigInt.asIntN(10, 256n));
  assertEquals(257n, BigInt.asIntN(10, 257n));
  assertEquals(510n, BigInt.asIntN(10, 510n));
  assertEquals(511n, BigInt.asIntN(10, 511n));
  assertEquals(-512n, BigInt.asIntN(10, 512n));
  assertEquals(-511n, BigInt.asIntN(10, 513n));
  assertEquals(-2n, BigInt.asIntN(10, 1022n));
  assertEquals(-1n, BigInt.asIntN(10, 1023n));
  assertEquals(0n, BigInt.asIntN(10, 1024n));
  assertEquals(1n, BigInt.asIntN(10, 1025n));
}{
  assertEquals(-254n, BigInt.asIntN(10, -254n));
  assertEquals(-255n, BigInt.asIntN(10, -255n));
  assertEquals(-256n, BigInt.asIntN(10, -256n));
  assertEquals(-257n, BigInt.asIntN(10, -257n));
  assertEquals(-510n, BigInt.asIntN(10, -510n));
  assertEquals(-511n, BigInt.asIntN(10, -511n));
  assertEquals(-512n, BigInt.asIntN(10, -512n));
  assertEquals(511n, BigInt.asIntN(10, -513n));
  assertEquals(2n, BigInt.asIntN(10, -1022n));
  assertEquals(1n, BigInt.asIntN(10, -1023n));
  assertEquals(0n, BigInt.asIntN(10, -1024n));
  assertEquals(-1n, BigInt.asIntN(10, -1025n));
}{
  assertEquals(0n, BigInt.asIntN(0, 0n));
  assertEquals(0n, BigInt.asIntN(1, 0n));
  assertEquals(0n, BigInt.asIntN(16, 0n));
  assertEquals(0n, BigInt.asIntN(31, 0n));
  assertEquals(0n, BigInt.asIntN(32, 0n));
  assertEquals(0n, BigInt.asIntN(33, 0n));
  assertEquals(0n, BigInt.asIntN(63, 0n));
  assertEquals(0n, BigInt.asIntN(64, 0n));
  assertEquals(0n, BigInt.asIntN(65, 0n));
  assertEquals(0n, BigInt.asIntN(127, 0n));
  assertEquals(0n, BigInt.asIntN(128, 0n));
  assertEquals(0n, BigInt.asIntN(129, 0n));
}{
  assertEquals(0n, BigInt.asIntN(0, 42n));
  assertEquals(0n, BigInt.asIntN(1, 42n));
  assertEquals(42n, BigInt.asIntN(16, 42n));
  assertEquals(42n, BigInt.asIntN(31, 42n));
  assertEquals(42n, BigInt.asIntN(32, 42n));
  assertEquals(42n, BigInt.asIntN(33, 42n));
  assertEquals(42n, BigInt.asIntN(63, 42n));
  assertEquals(42n, BigInt.asIntN(64, 42n));
  assertEquals(42n, BigInt.asIntN(65, 42n));
  assertEquals(42n, BigInt.asIntN(127, 42n));
  assertEquals(42n, BigInt.asIntN(128, 42n));
  assertEquals(42n, BigInt.asIntN(129, 42n));
}{
  assertEquals(0n, BigInt.asIntN(0, -42n));
  assertEquals(0n, BigInt.asIntN(1, -42n));
  assertEquals(-42n, BigInt.asIntN(16, -42n));
  assertEquals(-42n, BigInt.asIntN(31, -42n));
  assertEquals(-42n, BigInt.asIntN(32, -42n));
  assertEquals(-42n, BigInt.asIntN(33, -42n));
  assertEquals(-42n, BigInt.asIntN(63, -42n));
  assertEquals(-42n, BigInt.asIntN(64, -42n));
  assertEquals(-42n, BigInt.asIntN(65, -42n));
  assertEquals(-42n, BigInt.asIntN(127, -42n));
  assertEquals(-42n, BigInt.asIntN(128, -42n));
  assertEquals(-42n, BigInt.asIntN(129, -42n));
}{
  assertEquals(0n, BigInt.asIntN(0, 4294967295n));
  assertEquals(-1n, BigInt.asIntN(1, 4294967295n));
  assertEquals(-1n, BigInt.asIntN(16, 4294967295n));
  assertEquals(-1n, BigInt.asIntN(31, 4294967295n));
  assertEquals(-1n, BigInt.asIntN(32, 4294967295n));
  assertEquals(4294967295n, BigInt.asIntN(33, 4294967295n));
  assertEquals(4294967295n, BigInt.asIntN(63, 4294967295n));
  assertEquals(4294967295n, BigInt.asIntN(64, 4294967295n));
  assertEquals(4294967295n, BigInt.asIntN(65, 4294967295n));
  assertEquals(4294967295n, BigInt.asIntN(127, 4294967295n));
  assertEquals(4294967295n, BigInt.asIntN(128, 4294967295n));
  assertEquals(4294967295n, BigInt.asIntN(129, 4294967295n));
}{
  assertEquals(0n, BigInt.asIntN(0, -4294967295n));
  assertEquals(-1n, BigInt.asIntN(1, -4294967295n));
  assertEquals(1n, BigInt.asIntN(16, -4294967295n));
  assertEquals(1n, BigInt.asIntN(31, -4294967295n));
  assertEquals(1n, BigInt.asIntN(32, -4294967295n));
  assertEquals(-4294967295n, BigInt.asIntN(33, -4294967295n));
  assertEquals(-4294967295n, BigInt.asIntN(63, -4294967295n));
  assertEquals(-4294967295n, BigInt.asIntN(64,-4294967295n));
  assertEquals(-4294967295n, BigInt.asIntN(65, -4294967295n));
  assertEquals(-4294967295n, BigInt.asIntN(127, -4294967295n));
  assertEquals(-4294967295n, BigInt.asIntN(128, -4294967295n));
  assertEquals(-4294967295n, BigInt.asIntN(129, -4294967295n));
}{
  assertEquals(42n, BigInt.asIntN(2**32, 42n));
  assertEquals(4294967295n, BigInt.asIntN(2**32, 4294967295n));
  assertEquals(4294967296n, BigInt.asIntN(2**32, 4294967296n));
  assertEquals(4294967297n, BigInt.asIntN(2**32, 4294967297n));
}{
  assertThrows(() => BigInt.asIntN(2n, 12n), TypeError);
  assertThrows(() => BigInt.asIntN(-1, 0n), RangeError);
  assertThrows(() => BigInt.asIntN(2**53, 0n), RangeError);
  assertEquals(0n, BigInt.asIntN({}, 12n));
  assertEquals(0n, BigInt.asIntN(2.9999, 12n));
  assertEquals(-4n, BigInt.asIntN(3.1234, 12n));
}{
  assertThrows(() => BigInt.asIntN(3, 12), TypeError);
  assertEquals(-4n, BigInt.asIntN(3, "12"));
  assertEquals(0x123456789abcdefn,
               BigInt.asIntN(64, 0xabcdef0123456789abcdefn));
}

// BigInt.asUintN
{
  assertEquals(2, BigInt.asUintN.length);
}{
  assertEquals(7n, BigInt.asUintN(3, 15n));
  assertEquals(6n, BigInt.asUintN(3, 14n));
  assertEquals(5n, BigInt.asUintN(3, 13n));
  assertEquals(4n, BigInt.asUintN(3, 12n));
  assertEquals(3n, BigInt.asUintN(3, 11n));
  assertEquals(2n, BigInt.asUintN(3, 10n));
  assertEquals(1n, BigInt.asUintN(3, 9n));
  assertEquals(0n, BigInt.asUintN(3, 8n));
  assertEquals(7n, BigInt.asUintN(3, 7n));
  assertEquals(6n, BigInt.asUintN(3, 6n));
  assertEquals(5n, BigInt.asUintN(3, 5n));
  assertEquals(4n, BigInt.asUintN(3, 4n));
  assertEquals(3n, BigInt.asUintN(3, 3n));
  assertEquals(2n, BigInt.asUintN(3, 2n));
  assertEquals(1n, BigInt.asUintN(3, 1n));
  assertEquals(0n, BigInt.asUintN(3, 0n));
  assertEquals(7n, BigInt.asUintN(3, -1n));
  assertEquals(6n, BigInt.asUintN(3, -2n));
  assertEquals(5n, BigInt.asUintN(3, -3n));
  assertEquals(4n, BigInt.asUintN(3, -4n));
  assertEquals(3n, BigInt.asUintN(3, -5n));
  assertEquals(2n, BigInt.asUintN(3, -6n));
  assertEquals(1n, BigInt.asUintN(3, -7n));
  assertEquals(0n, BigInt.asUintN(3, -8n));
  assertEquals(7n, BigInt.asUintN(3, -9n));
  assertEquals(6n, BigInt.asUintN(3, -10n));
  assertEquals(5n, BigInt.asUintN(3, -11n));
  assertEquals(4n, BigInt.asUintN(3, -12n));
  assertEquals(3n, BigInt.asUintN(3, -13n));
  assertEquals(2n, BigInt.asUintN(3, -14n));
  assertEquals(1n, BigInt.asUintN(3, -15n));
}{
  assertEquals(254n, BigInt.asUintN(10, 254n));
  assertEquals(255n, BigInt.asUintN(10, 255n));
  assertEquals(256n, BigInt.asUintN(10, 256n));
  assertEquals(257n, BigInt.asUintN(10, 257n));
  assertEquals(510n, BigInt.asUintN(10, 510n));
  assertEquals(511n, BigInt.asUintN(10, 511n));
  assertEquals(512n, BigInt.asUintN(10, 512n));
  assertEquals(513n, BigInt.asUintN(10, 513n));
  assertEquals(1022n, BigInt.asUintN(10, 1022n));
  assertEquals(1023n, BigInt.asUintN(10, 1023n));
  assertEquals(0n, BigInt.asUintN(10, 1024n));
  assertEquals(1n, BigInt.asUintN(10, 1025n));
}{
  assertEquals(1024n - 254n, BigInt.asUintN(10, -254n));
  assertEquals(1024n - 255n, BigInt.asUintN(10, -255n));
  assertEquals(1024n - 256n, BigInt.asUintN(10, -256n));
  assertEquals(1024n - 257n, BigInt.asUintN(10, -257n));
  assertEquals(1024n - 510n, BigInt.asUintN(10, -510n));
  assertEquals(1024n - 511n, BigInt.asUintN(10, -511n));
  assertEquals(1024n - 512n, BigInt.asUintN(10, -512n));
  assertEquals(1024n - 513n, BigInt.asUintN(10, -513n));
  assertEquals(1024n - 1022n, BigInt.asUintN(10, -1022n));
  assertEquals(1024n - 1023n, BigInt.asUintN(10, -1023n));
  assertEquals(0n, BigInt.asUintN(10, -1024n));
  assertEquals(1023n, BigInt.asUintN(10, -1025n));
}{
  assertEquals(0n, BigInt.asUintN(0, 0n));
  assertEquals(0n, BigInt.asUintN(1, 0n));
  assertEquals(0n, BigInt.asUintN(16, 0n));
  assertEquals(0n, BigInt.asUintN(31, 0n));
  assertEquals(0n, BigInt.asUintN(32, 0n));
  assertEquals(0n, BigInt.asUintN(33, 0n));
  assertEquals(0n, BigInt.asUintN(63, 0n));
  assertEquals(0n, BigInt.asUintN(64, 0n));
  assertEquals(0n, BigInt.asUintN(65, 0n));
  assertEquals(0n, BigInt.asUintN(127, 0n));
  assertEquals(0n, BigInt.asUintN(128, 0n));
  assertEquals(0n, BigInt.asUintN(129, 0n));
}{
  assertEquals(0n, BigInt.asUintN(0, 42n));
  assertEquals(0n, BigInt.asUintN(1, 42n));
  assertEquals(42n, BigInt.asUintN(16, 42n));
  assertEquals(42n, BigInt.asUintN(31, 42n));
  assertEquals(42n, BigInt.asUintN(32, 42n));
  assertEquals(42n, BigInt.asUintN(33, 42n));
  assertEquals(42n, BigInt.asUintN(63, 42n));
  assertEquals(42n, BigInt.asUintN(64, 42n));
  assertEquals(42n, BigInt.asUintN(65, 42n));
  assertEquals(42n, BigInt.asUintN(127, 42n));
  assertEquals(42n, BigInt.asUintN(128, 42n));
  assertEquals(42n, BigInt.asUintN(129, 42n));
}{
  assertEquals(0n, BigInt.asUintN(0, -42n));
  assertEquals(0n, BigInt.asUintN(1, -42n));
  assertEquals(65536n - 42n, BigInt.asUintN(16, -42n));
  assertEquals(2147483648n - 42n, BigInt.asUintN(31, -42n));
  assertEquals(4294967296n - 42n, BigInt.asUintN(32, -42n));
  assertEquals(8589934592n - 42n, BigInt.asUintN(33, -42n));
  assertEquals(9223372036854775808n - 42n, BigInt.asUintN(63, -42n));
  assertEquals(18446744073709551616n - 42n, BigInt.asUintN(64, -42n));
  assertEquals(36893488147419103232n - 42n, BigInt.asUintN(65, -42n));
  assertEquals(2n**127n - 42n, BigInt.asUintN(127, -42n));
  assertEquals(2n**128n - 42n, BigInt.asUintN(128, -42n));
  assertEquals(2n**129n - 42n, BigInt.asUintN(129, -42n));
}{
  assertEquals(0n, BigInt.asUintN(0, 4294967295n));
  assertEquals(1n, BigInt.asUintN(1, 4294967295n));
  assertEquals(65535n, BigInt.asUintN(16, 4294967295n));
  assertEquals(2147483647n, BigInt.asUintN(31, 4294967295n));
  assertEquals(4294967295n, BigInt.asUintN(32, 4294967295n));
  assertEquals(4294967295n, BigInt.asUintN(33, 4294967295n));
  assertEquals(4294967295n, BigInt.asUintN(63, 4294967295n));
  assertEquals(4294967295n, BigInt.asUintN(64, 4294967295n));
  assertEquals(4294967295n, BigInt.asUintN(65, 4294967295n));
  assertEquals(4294967295n, BigInt.asUintN(127, 4294967295n));
  assertEquals(4294967295n, BigInt.asUintN(128, 4294967295n));
  assertEquals(4294967295n, BigInt.asUintN(129, 4294967295n));
}{
  assertEquals(0n, BigInt.asUintN(0, -4294967295n));
  assertEquals(1n, BigInt.asUintN(1, -4294967295n));
  assertEquals(1n, BigInt.asUintN(16, -4294967295n));
  assertEquals(1n, BigInt.asUintN(31, -4294967295n));
  assertEquals(1n, BigInt.asUintN(32, -4294967295n));
  assertEquals(8589934592n - 4294967295n, BigInt.asUintN(33, -4294967295n));
  assertEquals(9223372036854775808n - 4294967295n,
      BigInt.asUintN(63, -4294967295n));
  assertEquals(18446744073709551616n - 4294967295n,
      BigInt.asUintN(64,-4294967295n));
  assertEquals(36893488147419103232n - 4294967295n,
      BigInt.asUintN(65, -4294967295n));
  assertEquals(2n**127n - 4294967295n, BigInt.asUintN(127, -4294967295n));
  assertEquals(2n**128n - 4294967295n, BigInt.asUintN(128, -4294967295n));
  assertEquals(2n**129n - 4294967295n, BigInt.asUintN(129, -4294967295n));
}{
  assertEquals(42n, BigInt.asUintN(2**32, 42n));
  assertEquals(4294967295n, BigInt.asUintN(2**32, 4294967295n));
  assertEquals(4294967296n, BigInt.asUintN(2**32, 4294967296n));
  assertEquals(4294967297n, BigInt.asUintN(2**32, 4294967297n));
}{
  assertEquals(0x7234567812345678n, BigInt.asUintN(63, 0xf234567812345678n));
}{
  assertThrows(() => BigInt.asUintN(2n, 12n), TypeError);
  assertThrows(() => BigInt.asUintN(-1, 0n), RangeError);
  assertThrows(() => BigInt.asUintN(2**53, 0n), RangeError);
  assertEquals(0n, BigInt.asUintN({}, 12n));
  assertEquals(0n, BigInt.asUintN(2.9999, 12n));
  assertEquals(4n, BigInt.asUintN(3.1234, 12n));
}{
  assertThrows(() => BigInt.asUintN(3, 12), TypeError);
  assertEquals(4n, BigInt.asUintN(3, "12"));
}
