// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-intl-number-format-v3

let nf = new Intl.NumberFormat("en");
// Basic case
assertEquals("123", nf.format("123"));
assertEquals("123.45", nf.format("123.45"));
assertEquals("123", nf.format("+123"));
assertEquals("123.45", nf.format("+123.45"));
assertEquals("-123", nf.format("-123"));
assertEquals("-123.45", nf.format("-123.45"));

// with _
assertEquals("NaN", nf.format("1_2_3_4"));
assertEquals("NaN", nf.format("1_2.3_4"));
assertEquals("NaN", nf.format("1_2.34"));
assertEquals("NaN", nf.format("12.3_4"));
assertEquals("NaN", nf.format(".1_2_3"));
assertEquals("NaN", nf.format("123e1_2"));
assertEquals("NaN", nf.format("123e-1_2"));
assertEquals("NaN", nf.format("1.23e1_2"));
assertEquals("NaN", nf.format("12.3e-1_2"));
assertEquals("NaN", nf.format("12.3e+1_2"));

let str_white_space = " \u0009\u000b\u000c\ufeff\u000a\u000d\u2028\u2029";
// With StrWhiteSpace_opt only
assertEquals("0", nf.format(str_white_space));

// With StrWhiteSpace_opt prefix/postfix
assertEquals("123", nf.format(str_white_space + "123" + str_white_space));
assertEquals("123.45", nf.format(str_white_space + "123.45" + str_white_space));
assertEquals("123", nf.format(str_white_space + "+123" + str_white_space));
assertEquals("123.45",
    nf.format(str_white_space + "+123.45" + str_white_space));
assertEquals("-123", nf.format(str_white_space + "-123" + str_white_space));
assertEquals("-123.45",
    nf.format(str_white_space + "-123.45" + str_white_space));

// NonDecimalIntegerLiteral
assertEquals("10", nf.format("0b1010"));
assertEquals("11", nf.format("0B1011"));
assertEquals("10", nf.format("0o12"));
assertEquals("11", nf.format("0O13"));
assertEquals("10", nf.format("0x000A"));
assertEquals("11", nf.format("0X00B"));
// With StrWhiteSpace_opt prefix/postfix
assertEquals("10", nf.format(str_white_space + "0b1010" + str_white_space));
assertEquals("11", nf.format(str_white_space + "0B1011" + str_white_space));
assertEquals("10", nf.format(str_white_space + "0o12" + str_white_space));
assertEquals("11", nf.format(str_white_space + "0O13" + str_white_space));
assertEquals("10", nf.format(str_white_space + "0x000A" + str_white_space));
assertEquals("11", nf.format(str_white_space + "0X00B" + str_white_space));
// Very large NonDecimalIntegerLiteral
assertEquals("1,208,925,819,614,629,174,706,175",
    nf.format("0xFFFFFFFFFFFFFFFFFFFF"));
assertEquals("1,208,925,819,614,629,174,706,174",
    nf.format("0XFFFFFFFFFFFFFFFFFFFE"));

// NaN

// Infinity
assertEquals("∞", nf.format("Infinity"));
assertEquals("∞", nf.format("+Infinity"));
assertEquals("-∞", nf.format("-Infinity"));
// With StrWhiteSpace_opt prefix/postfix
assertEquals("∞", nf.format(str_white_space + "Infinity" + str_white_space));
assertEquals("∞", nf.format(str_white_space + "+Infinity" + str_white_space));
assertEquals("-∞", nf.format(str_white_space + "-Infinity" + str_white_space));

// Extra SPACE
assertEquals("NaN", nf.format("+ Infinity"));
assertEquals("NaN", nf.format("- Infinity"));

// DecimalDigits . DecimalDigits_opt ExponentPart_opt
assertEquals("123.45", nf.format("12345e-2"));
assertEquals("123.45", nf.format("+12345e-2"));
assertEquals("-123.45", nf.format("-12345e-2"));
assertEquals("123.45", nf.format("1.2345e2"));
assertEquals("123.45", nf.format("1.2345e+2"));
assertEquals("-123.45", nf.format("-1.2345e2"));
assertEquals("-123.45", nf.format("-1.2345e+2"));
// With StrWhiteSpace_opt prefix/postfix
assertEquals("123.45",
    nf.format(str_white_space + "12345e-2" + str_white_space));
assertEquals("123.45",
    nf.format(str_white_space + "+12345e-2" + str_white_space));
assertEquals("-123.45",
    nf.format(str_white_space + "-12345e-2" + str_white_space));
assertEquals("123.45",
    nf.format(str_white_space + "1.2345e2" + str_white_space));
assertEquals("123.45",
    nf.format(str_white_space + "1.2345e+2" + str_white_space));
assertEquals("-123.45", nf.format("-1.2345e2"));
assertEquals("-123.45", nf.format("-1.2345e+2"));

// . DecimalDigits ExponentPart_opt
assertEquals("123.45", nf.format(".12345e3"));
assertEquals("123.45", nf.format("+.12345e+3"));
assertEquals("-123.45", nf.format("-.12345e3"));
assertEquals("-123.45", nf.format("-.12345e+3"));
// With StrWhiteSpace_opt prefix/postfix
assertEquals("123.45",
    nf.format(str_white_space + ".12345e3" + str_white_space));
assertEquals("123.45",
    nf.format(str_white_space + "+.12345e+3" + str_white_space));
assertEquals("-123.45",
    nf.format(str_white_space + "-.12345e3" + str_white_space));
assertEquals("-123.45",
    nf.format(str_white_space + "-.12345e+3" + str_white_space));

assertEquals("1,234,567,890,123,456,789,012,345,678,901,234,567,890,123",
    nf.format("1234567890123456789012345678901234567890123"));
assertEquals("-1,234,567,890,123,456,789,012,345,678,901,234,567,890,123",
    nf.format("-1234567890123456789012345678901234567890123"));
assertEquals(
    "1,234,567,890,123,456,789,012,345,678,901,234,567,890,123,000,000,000,000",
    nf.format("1234567890123456789012345678901234567890123e12"));

// With StrWhiteSpace_opt prefix/postfix
assertEquals("1,234,567,890,123,456,789,012,345,678,901,234,567,890,123",
    nf.format(str_white_space +
      "1234567890123456789012345678901234567890123" + str_white_space));
assertEquals("-1,234,567,890,123,456,789,012,345,678,901,234,567,890,123",
    nf.format(str_white_space +
      "-1234567890123456789012345678901234567890123" + str_white_space));
assertEquals(
    "1,234,567,890,123,456,789,012,345,678,901,234,567,890,123,000,000,000,000",
    nf.format(str_white_space +
      "1234567890123456789012345678901234567890123e12" + str_white_space));
