// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

var common = require('../common');
var assert = require('assert');

// does node think that i18n was enabled?
var enablei18n = process.config.variables.v8_enable_i18n_support;
if (enablei18n === undefined) {
    enablei18n = false;
}

// is the Intl object present?
var haveIntl = (global.Intl != undefined);

// Returns true if no specific locale ids were configured (i.e. "all")
// Else, returns true if loc is in the configured list
// Else, returns false
function haveLocale(loc) {
  var locs = process.config.variables.icu_locales.split(',');
  return locs.indexOf(loc) !== -1;
}

if (!haveIntl) {
  var erMsg =
      '"Intl" object is NOT present but v8_enable_i18n_support is ' +
      enablei18n;
  assert.equal(enablei18n, false, erMsg);
  console.log('Skipping Intl tests because Intl object not present.');

} else {
  var erMsg =
    '"Intl" object is present but v8_enable_i18n_support is ' +
    enablei18n +
    '. Is this test out of date?';
  assert.equal(enablei18n, true, erMsg);

  // Construct a new date at the beginning of Unix time
  var date0 = new Date(0);

  // Use the GMT time zone
  var GMT = 'Etc/GMT';

  // Construct an English formatter. Should format to "Jan 70"
  var dtf =
      new Intl.DateTimeFormat(['en'],
                              {timeZone: GMT, month: 'short', year: '2-digit'});

  // If list is specified and doesn't contain 'en' then return.
  if (process.config.variables.icu_locales && !haveLocale('en')) {
    console.log('Skipping detailed Intl tests because English is not listed ' +
                'as supported.');
    // Smoke test. Does it format anything, or fail?
    console.log('Date(0) formatted to: ' + dtf.format(date0));
    return;
  }

  // Check with toLocaleString
  var localeString = dtf.format(date0);
  assert.equal(localeString, 'Jan 70');

  // Options to request GMT
  var optsGMT = {timeZone: GMT};

  // Test format
  localeString = date0.toLocaleString(['en'], optsGMT);
  assert.equal(localeString, '1/1/1970, 12:00:00 AM');

  // number format
  assert.equal(new Intl.NumberFormat(['en']).format(12345.67890), '12,345.679');

  var collOpts = { sensitivity: 'base', ignorePunctuation: true };
  var coll = new Intl.Collator(['en'], collOpts);

  assert.equal(coll.compare('blackbird', 'black-bird'), 0,
               'ignore punctuation failed');
  assert.equal(coll.compare('blackbird', 'red-bird'), -1,
               'compare less failed');
  assert.equal(coll.compare('bluebird', 'blackbird'), 1,
               'compare greater failed');
  assert.equal(coll.compare('Bluebird', 'bluebird'), 0,
               'ignore case failed');
  assert.equal(coll.compare('\ufb03', 'ffi'), 0,
               'ffi ligature (contraction) failed');
}
