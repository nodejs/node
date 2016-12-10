'use strict';
const common = require('../common');
const assert = require('assert');

// does node think that i18n was enabled?
let enablei18n = process.config.variables.v8_enable_i18n_support;
if (enablei18n === undefined) {
  enablei18n = 0;
}

// Returns true if no specific locale ids were configured (i.e. "all")
// Else, returns true if loc is in the configured list
// Else, returns false
function haveLocale(loc) {
  const locs = process.config.variables.icu_locales.split(',');
  return locs.indexOf(loc) !== -1;
}

if (!common.hasIntl) {
  const erMsg =
      '"Intl" object is NOT present but v8_enable_i18n_support is ' +
      enablei18n;
  assert.strictEqual(enablei18n, 0, erMsg);
  common.skip('Intl tests because Intl object not present.');

} else {
  const erMsg =
    '"Intl" object is present but v8_enable_i18n_support is ' +
    enablei18n +
    '. Is this test out of date?';
  assert.strictEqual(enablei18n, 1, erMsg);

  // Construct a new date at the beginning of Unix time
  const date0 = new Date(0);

  // Use the GMT time zone
  const GMT = 'Etc/GMT';

  // Construct an English formatter. Should format to "Jan 70"
  const dtf =
      new Intl.DateTimeFormat(['en'],
                              {timeZone: GMT, month: 'short', year: '2-digit'});

  // If list is specified and doesn't contain 'en' then return.
  if (process.config.variables.icu_locales && !haveLocale('en')) {
    common.skip('detailed Intl tests because English is not ' +
                'listed as supported.');
    // Smoke test. Does it format anything, or fail?
    console.log('Date(0) formatted to: ' + dtf.format(date0));
    return;
  }

  // Check casing
  {
    assert.strictEqual('I'.toLocaleLowerCase('tr'), 'ı');
  }

  // Check with toLocaleString
  {
    const localeString = dtf.format(date0);
    assert.strictEqual(localeString, 'Jan 70');
  }
  // Options to request GMT
  const optsGMT = {timeZone: GMT};

  // Test format
  {
    const localeString = date0.toLocaleString(['en'], optsGMT);
    assert.strictEqual(localeString, '1/1/1970, 12:00:00 AM');
  }
  // number format
  const numberFormat = new Intl.NumberFormat(['en']).format(12345.67890);
  assert.strictEqual(numberFormat, '12,345.679');

  const collOpts = { sensitivity: 'base', ignorePunctuation: true };
  const coll = new Intl.Collator(['en'], collOpts);

  assert.strictEqual(coll.compare('blackbird', 'black-bird'), 0,
                     'ignore punctuation failed');
  assert.strictEqual(coll.compare('blackbird', 'red-bird'), -1,
                     'compare less failed');
  assert.strictEqual(coll.compare('bluebird', 'blackbird'), 1,
                     'compare greater failed');
  assert.strictEqual(coll.compare('Bluebird', 'bluebird'), 0,
                     'ignore case failed');
  assert.strictEqual(coll.compare('\ufb03', 'ffi'), 0,
                     'ffi ligature (contraction) failed');
}
