// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-number-format-to-parts

// Adapted from Gecko's js/src/tests/Intl/NumberFormat/formatToParts.js,
// which was dedicated to the public domain.

// NOTE: Some of these tests exercise standard behavior (e.g. that format and
//       formatToParts expose the same formatted string).  But much of this,
//       like the exact-formatted-string expectations, is technically
//       implementation-dependent.  This is necessary as a practical matter to
//       properly test the conversion from ICU's nested-field exposure to
//       ECMA-402's sequential-parts exposure.

if (this.Intl) {

  function GenericPartCreator(type)
  {
    return function(str) { return { type, value: str }; };
  }

  var Nan = GenericPartCreator("nan");
  var Inf = GenericPartCreator("infinity");
  var Integer = GenericPartCreator("integer");
  var Group = GenericPartCreator("group");
  var Decimal = GenericPartCreator("decimal");
  var Fraction = GenericPartCreator("fraction");
  var MinusSign = GenericPartCreator("minusSign");
  var PlusSign = GenericPartCreator("plusSign");
  var PercentSign = GenericPartCreator("percentSign");
  var Currency = GenericPartCreator("currency");
  var Literal = GenericPartCreator("literal");

  function assertParts(nf, x, expected)
  {
    var parts = nf.formatToParts(x);
    assertEquals(nf.format(x),
                 parts.map(part => part.value).join(""));

    var len = parts.length;
    assertEquals(expected.length, len);
    for (var i = 0; i < len; i++)
    {
      assertEquals(expected[i].type, parts[i].type);
      assertEquals(expected[i].value, parts[i].value);
    }
  }

  //-----------------------------------------------------------------------------

  // Test behavior of a currency with code formatting.
  var usdCodeOptions =
    {
      style: "currency",
      currency: "USD",
      currencyDisplay: "code",
      minimumFractionDigits: 0,
      maximumFractionDigits: 0,
    };
  var usDollarsCode = new Intl.NumberFormat("en-US", usdCodeOptions);

  assertParts(usDollarsCode, 25,
              [Currency("USD"), Integer("25")]);

  // ISO 4217 currency codes are formed from an ISO 3166-1 alpha-2 country code
  // followed by a third letter.  ISO 3166 guarantees that no country code
  // starting with "X" will ever be assigned.  Stepping carefully around a few
  // 4217-designated special "currencies", XQQ will never have a representation.
  // Thus, yes: this really is specified to work, as unrecognized or unsupported
  // codes pass into the string unmodified.
  var xqqCodeOptions =
    {
      style: "currency",
      currency: "XQQ",
      currencyDisplay: "code",
      minimumFractionDigits: 0,
      maximumFractionDigits: 0,
    };
  var xqqMoneyCode = new Intl.NumberFormat("en-US", xqqCodeOptions);

  assertParts(xqqMoneyCode, 25,
              [Currency("XQQ"), Integer("25")]);

  // Test currencyDisplay: "name".
  var usdNameOptions =
    {
      style: "currency",
      currency: "USD",
      currencyDisplay: "name",
      minimumFractionDigits: 0,
      maximumFractionDigits: 0,
    };
  var usDollarsName = new Intl.NumberFormat("en-US", usdNameOptions);

  assertParts(usDollarsName, 25,
              [Integer("25"), Literal(" "), Currency("US dollars")]);

  var usdNameGroupingOptions =
    {
      style: "currency",
      currency: "USD",
      currencyDisplay: "name",
      minimumFractionDigits: 0,
      maximumFractionDigits: 0,
    };
  var usDollarsNameGrouping =
    new Intl.NumberFormat("en-US", usdNameGroupingOptions);

  assertParts(usDollarsNameGrouping, 12345678,
              [Integer("12"),
               Group(","),
               Integer("345"),
               Group(","),
               Integer("678"),
               Literal(" "),
               Currency("US dollars")]);

  // But if the implementation doesn't recognize the currency, the provided code
  // is used in place of a proper name, unmolested.
  var xqqNameOptions =
    {
      style: "currency",
      currency: "XQQ",
      currencyDisplay: "name",
      minimumFractionDigits: 0,
      maximumFractionDigits: 0,
    };
  var xqqMoneyName = new Intl.NumberFormat("en-US", xqqNameOptions);

  assertParts(xqqMoneyName, 25,
              [Integer("25"), Literal(" "), Currency("XQQ")]);

  // Test some currencies with fractional components.

  var usdNameFractionOptions =
    {
      style: "currency",
      currency: "USD",
      currencyDisplay: "name",
      minimumFractionDigits: 2,
      maximumFractionDigits: 2,
    };
  var usdNameFractionFormatter =
    new Intl.NumberFormat("en-US", usdNameFractionOptions);

  // The US national surplus (i.e. debt) as of October 18, 2016.
  // (Replicating data from a comment in Intl.cpp.)
  var usNationalSurplus = -19766580028249.41;

  assertParts(usdNameFractionFormatter, usNationalSurplus,
              [MinusSign("-"),
               Integer("19"),
               Group(","),
               Integer("766"),
               Group(","),
               Integer("580"),
               Group(","),
               Integer("028"),
               Group(","),
               Integer("249"),
               Decimal("."),
               Fraction("41"),
               Literal(" "),
               Currency("US dollars")]);

  // Percents in various forms.

  var usPercentOptions =
    {
      style: "percent",
      minimumFractionDigits: 1,
      maximumFractionDigits: 1,
    };
  var usPercentFormatter =
    new Intl.NumberFormat("en-US", usPercentOptions);

  assertParts(usPercentFormatter, 0.375,
              [Integer("37"), Decimal("."), Fraction("5"), PercentSign("%")]);

  assertParts(usPercentFormatter, -1284.375,
              [MinusSign("-"),
               Integer("128"),
               Group(","),
               Integer("437"),
               Decimal("."),
               Fraction("5"),
               PercentSign("%")]);

  assertParts(usPercentFormatter, NaN,
              [Nan("NaN")]);

  assertParts(usPercentFormatter, Infinity,
              [Inf("∞"), PercentSign("%")]);

  assertParts(usPercentFormatter, -Infinity,
              [MinusSign("-"), Inf("∞"), PercentSign("%")]);

  var arPercentOptions =
    {
      style: "percent",
      minimumFractionDigits: 2,
    };
  var arPercentFormatter =
    new Intl.NumberFormat("ar-IQ", arPercentOptions);

  assertParts(arPercentFormatter, -135.32,
              [MinusSign("\u{061C}-"),
               Integer("١٣"),
               Group("٬"),
               Integer("٥٣٢"),
               Decimal("٫"),
               Fraction("٠٠"),
               Literal("\xA0"),
               PercentSign("٪\u{061C}")]);

  // Decimals.

  var usDecimalOptions =
    {
      style: "decimal",
      maximumFractionDigits: 7 // minimum defaults to 0
    };
  var usDecimalFormatter =
    new Intl.NumberFormat("en-US", usDecimalOptions);

  assertParts(usDecimalFormatter, 42,
              [Integer("42")]);

  assertParts(usDecimalFormatter, 1337,
              [Integer("1"), Group(","), Integer("337")]);

  assertParts(usDecimalFormatter, -6.25,
              [MinusSign("-"), Integer("6"), Decimal("."), Fraction("25")]);

  assertParts(usDecimalFormatter, -1376.25,
              [MinusSign("-"),
               Integer("1"),
               Group(","),
               Integer("376"),
               Decimal("."),
               Fraction("25")]);

  assertParts(usDecimalFormatter, 124816.8359375,
              [Integer("124"),
               Group(","),
               Integer("816"),
               Decimal("."),
               Fraction("8359375")]);

  var usNoGroupingDecimalOptions =
    {
      style: "decimal",
      useGrouping: false,
      maximumFractionDigits: 7 // minimum defaults to 0
    };
  var usNoGroupingDecimalFormatter =
    new Intl.NumberFormat("en-US", usNoGroupingDecimalOptions);

  assertParts(usNoGroupingDecimalFormatter, 1337,
              [Integer("1337")]);

  assertParts(usNoGroupingDecimalFormatter, -6.25,
              [MinusSign("-"), Integer("6"), Decimal("."), Fraction("25")]);

  assertParts(usNoGroupingDecimalFormatter, -1376.25,
              [MinusSign("-"),
               Integer("1376"),
               Decimal("."),
               Fraction("25")]);

  assertParts(usNoGroupingDecimalFormatter, 124816.8359375,
              [Integer("124816"),
               Decimal("."),
               Fraction("8359375")]);

  var deDecimalOptions =
    {
      style: "decimal",
      maximumFractionDigits: 7 // minimum defaults to 0
    };
  var deDecimalFormatter =
    new Intl.NumberFormat("de-DE", deDecimalOptions);

  assertParts(deDecimalFormatter, 42,
              [Integer("42")]);

  assertParts(deDecimalFormatter, 1337,
              [Integer("1"), Group("."), Integer("337")]);

  assertParts(deDecimalFormatter, -6.25,
              [MinusSign("-"), Integer("6"), Decimal(","), Fraction("25")]);

  assertParts(deDecimalFormatter, -1376.25,
              [MinusSign("-"),
               Integer("1"),
               Group("."),
               Integer("376"),
               Decimal(","),
               Fraction("25")]);

  assertParts(deDecimalFormatter, 124816.8359375,
              [Integer("124"),
               Group("."),
               Integer("816"),
               Decimal(","),
               Fraction("8359375")]);

  var deNoGroupingDecimalOptions =
    {
      style: "decimal",
      useGrouping: false,
      maximumFractionDigits: 7 // minimum defaults to 0
    };
  var deNoGroupingDecimalFormatter =
    new Intl.NumberFormat("de-DE", deNoGroupingDecimalOptions);

  assertParts(deNoGroupingDecimalFormatter, 1337,
              [Integer("1337")]);

  assertParts(deNoGroupingDecimalFormatter, -6.25,
              [MinusSign("-"), Integer("6"), Decimal(","), Fraction("25")]);

  assertParts(deNoGroupingDecimalFormatter, -1376.25,
              [MinusSign("-"),
               Integer("1376"),
               Decimal(","),
               Fraction("25")]);

  assertParts(deNoGroupingDecimalFormatter, 124816.8359375,
              [Integer("124816"),
               Decimal(","),
               Fraction("8359375")]);

}
