// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Tests side-effect-free evaluation with i18n enabled');

contextGroup.addScript(`
var someGlobalCollator = new Intl.Collator("en-Latn-US");
var someGlobalDateTimeFormat = new Intl.DateTimeFormat("en-Latn-US");
var someGlobalDisplayNames = new Intl.DisplayNames(["en-Latn-US"], {type: 'region'});
var someGlobalListFormat = new Intl.ListFormat('en', { style: 'long', type: 'conjunction' });
var someGlobalLocale = new Intl.Locale("en-Latn-US", {language: "es"});
var someGlobalNumberFormat = new Intl.NumberFormat('de-DE', { style: 'currency', currency: 'EUR' });
var someGlobalPluralRules = new Intl.PluralRules('en-US');
var someGlobalRelativeTimeFormat = new Intl.RelativeTimeFormat("en-US", {style: "narrow"});
`, 0, 0, 'foo.js');

const check = async (expression) => {
  const {result:{exceptionDetails}} = await Protocol.Runtime.evaluate({expression, throwOnSideEffect: true});
  InspectorTest.log(expression + ' : ' + (exceptionDetails ? 'throws' : 'ok'));
};

InspectorTest.runAsyncTestSuite([
  async function testCollator() {
    await Protocol.Runtime.enable();

    // static methods
    await check('Intl.Collator.supportedLocalesOf(["en-US"])');

    // constructor
    await check('new Intl.Collator("en-US")');

    // methods
    await check('someGlobalCollator.compare("foo", "bar")');
    await check('someGlobalCollator.resolvedOptions()');

    await Protocol.Runtime.disable();
  },

  async function testDateTimeFormat() {
    await Protocol.Runtime.enable();

    // static methods
    await check('Intl.DateTimeFormat.supportedLocalesOf(["en-US"])');

    // constructor
    await check('new Intl.DateTimeFormat("en-US")');

    // methods
    await check('someGlobalDateTimeFormat.format(new Date(2021, 5))');
    await check('someGlobalDateTimeFormat.formatToParts(new Date(2021, 5))');
    await check('someGlobalDateTimeFormat.resolvedOptions()');
    await check('someGlobalDateTimeFormat.formatRange(new Date(2021, 5), new Date(2022, 1))');
    await check('someGlobalDateTimeFormat.formatRangeToParts(new Date(2021, 5), new Date(2022, 1))');

    await Protocol.Runtime.disable();
  },

  async function testDisplayNames() {
    await Protocol.Runtime.enable();

    // static methods
    await check('Intl.DisplayNames.supportedLocalesOf(["en-US"])');

    // constructor
    await check('new Intl.DisplayNames(["en-US"], {type: "region"})');

    // methods
    await check('someGlobalDisplayNames.of("en")');
    await check('someGlobalDisplayNames.resolvedOptions()');

    await Protocol.Runtime.disable();
  },

  async function testIntl() {
    await Protocol.Runtime.enable();

    // static methods
    await check('Intl.getCanonicalLocales("en-US")');

    await Protocol.Runtime.disable();
  },

  async function testListFormat() {
    await Protocol.Runtime.enable();

    // static methods
    await check('Intl.ListFormat.supportedLocalesOf(["en-US"])');

    // constructor
    await check('new Intl.ListFormat("en", { style: "long", type: "conjunction" });')

    // methods
    await check('someGlobalListFormat.format(["a", "b"])');
    await check('someGlobalListFormat.formatToParts(["a", "b"])');
    await check('someGlobalListFormat.resolvedOptions()');

    await Protocol.Runtime.disable();
  },

  async function testLocale() {
    await Protocol.Runtime.enable();

    // constructor
    await check('new Intl.Locale("en-US")')

    // getters
    await check('someGlobalLocale.baseName');
    await check('someGlobalLocale.calendar');
    await check('someGlobalLocale.calendars');
    await check('someGlobalLocale.caseFirst');
    await check('someGlobalLocale.collation');
    await check('someGlobalLocale.hourCycle');
    await check('someGlobalLocale.hourCycles');
    await check('someGlobalLocale.language');
    await check('someGlobalLocale.numberingSystem');
    await check('someGlobalLocale.numberingSystems');
    await check('someGlobalLocale.numeric');
    await check('someGlobalLocale.region');
    await check('someGlobalLocale.script');
    await check('someGlobalLocale.textInfo');
    await check('someGlobalLocale.timeZones');
    await check('someGlobalLocale.weekInfo');

    // methods
    await check('someGlobalLocale.maximize()');
    await check('someGlobalLocale.minimize()');
    await check('someGlobalLocale.toString()');

    await Protocol.Runtime.disable();
  },

  async function testNumberFormat() {
    await Protocol.Runtime.enable();

    // static methods
    await check('Intl.NumberFormat.supportedLocalesOf(["en-US"])');

    // constructor
    await check('new Intl.NumberFormat("de-DE", { style: "currency", currency: "EUR" })');

    // methods
    await check('someGlobalNumberFormat.format(1)');
    await check('someGlobalNumberFormat.formatToParts(1)');
    await check('someGlobalNumberFormat.resolvedOptions()');

    await Protocol.Runtime.disable();
  },

  async function testPluralRules() {
    await Protocol.Runtime.enable();

    // static methods
    await check('Intl.PluralRules.supportedLocalesOf(["en-US"])');

    // constructor
    await check('new Intl.PluralRules("en-US")');

    // methods
    await check('someGlobalPluralRules.resolvedOptions()');
    await check('someGlobalPluralRules.select(42)');

    await Protocol.Runtime.disable();
  },

  async function testRelativeTimeFormat() {
    await Protocol.Runtime.enable();

    // static methods
    await check('Intl.RelativeTimeFormat.supportedLocalesOf(["en-US"])');

    // constructor
    await check('new Intl.RelativeTimeFormat("en-US", {style: "narrow"})');

    // methods
    await check('someGlobalRelativeTimeFormat.format(2, "day")');
    await check('someGlobalRelativeTimeFormat.formatToParts(2, "day")');
    await check('someGlobalRelativeTimeFormat.resolvedOptions()');

    await Protocol.Runtime.disable();
  }
]);
