// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// limitations under the License.

// ECMAScript 402 API implementation is broken into separate files for
// each service. The build system combines them together into one
// Intl namespace.

/**
 * Verifies that the input is a well-formed ISO 4217 currency code.
 * Don't uppercase to test. It could convert invalid code into a valid one.
 * For example \u00DFP (Eszett+P) becomes SSP.
 */
function isWellFormedCurrencyCode(currency) {
  return typeof currency == "string" &&
      currency.length == 3 &&
      currency.match(/[^A-Za-z]/) == null;
}


/**
 * Returns the valid digit count for a property, or throws RangeError on
 * a value out of the range.
 */
function getNumberOption(options, property, min, max, fallback) {
  var value = options[property];
  if (value !== undefined) {
    value = Number(value);
    if (isNaN(value) || value < min || value > max) {
      throw new RangeError(property + ' value is out of range.');
    }
    return Math.floor(value);
  }

  return fallback;
}


/**
 * Initializes the given object so it's a valid NumberFormat instance.
 * Useful for subclassing.
 */
function initializeNumberFormat(numberFormat, locales, options) {
  native function NativeJSCreateNumberFormat();

  if (numberFormat.hasOwnProperty('__initializedIntlObject')) {
    throw new TypeError('Trying to re-initialize NumberFormat object.');
  }

  if (options === undefined) {
    options = {};
  }

  var getOption = getGetOption(options, 'numberformat');

  var locale = resolveLocale('numberformat', locales, options);

  var internalOptions = {};
  defineWEProperty(internalOptions, 'style', getOption(
    'style', 'string', ['decimal', 'percent', 'currency'], 'decimal'));

  var currency = getOption('currency', 'string');
  if (currency !== undefined && !isWellFormedCurrencyCode(currency)) {
    throw new RangeError('Invalid currency code: ' + currency);
  }

  if (internalOptions.style === 'currency' && currency === undefined) {
    throw new TypeError('Currency code is required with currency style.');
  }

  var currencyDisplay = getOption(
      'currencyDisplay', 'string', ['code', 'symbol', 'name'], 'symbol');
  if (internalOptions.style === 'currency') {
    defineWEProperty(internalOptions, 'currency', currency.toUpperCase());
    defineWEProperty(internalOptions, 'currencyDisplay', currencyDisplay);
  }

  // Digit ranges.
  var mnid = getNumberOption(options, 'minimumIntegerDigits', 1, 21, 1);
  defineWEProperty(internalOptions, 'minimumIntegerDigits', mnid);

  var mnfd = getNumberOption(options, 'minimumFractionDigits', 0, 20, 0);
  defineWEProperty(internalOptions, 'minimumFractionDigits', mnfd);

  var mxfd = getNumberOption(options, 'maximumFractionDigits', mnfd, 20, 3);
  defineWEProperty(internalOptions, 'maximumFractionDigits', mxfd);

  var mnsd = options['minimumSignificantDigits'];
  var mxsd = options['maximumSignificantDigits'];
  if (mnsd !== undefined || mxsd !== undefined) {
    mnsd = getNumberOption(options, 'minimumSignificantDigits', 1, 21, 0);
    defineWEProperty(internalOptions, 'minimumSignificantDigits', mnsd);

    mxsd = getNumberOption(options, 'maximumSignificantDigits', mnsd, 21, 21);
    defineWEProperty(internalOptions, 'maximumSignificantDigits', mxsd);
  }

  // Grouping.
  defineWEProperty(internalOptions, 'useGrouping', getOption(
    'useGrouping', 'boolean', undefined, true));

  // ICU prefers options to be passed using -u- extension key/values for
  // number format, so we need to build that.
  var extensionMap = parseExtension(locale.extension);
  var extension = setOptions(options, extensionMap, NUMBER_FORMAT_KEY_MAP,
                             getOption, internalOptions);

  var requestedLocale = locale.locale + extension;
  var resolved = Object.defineProperties({}, {
    currency: {writable: true},
    currencyDisplay: {writable: true},
    locale: {writable: true},
    maximumFractionDigits: {writable: true},
    minimumFractionDigits: {writable: true},
    minimumIntegerDigits: {writable: true},
    numberingSystem: {writable: true},
    requestedLocale: {value: requestedLocale, writable: true},
    style: {value: internalOptions.style, writable: true},
    useGrouping: {writable: true}
  });
  if (internalOptions.hasOwnProperty('minimumSignificantDigits')) {
    defineWEProperty(resolved, 'minimumSignificantDigits', undefined);
  }
  if (internalOptions.hasOwnProperty('maximumSignificantDigits')) {
    defineWEProperty(resolved, 'maximumSignificantDigits', undefined);
  }
  var formatter = NativeJSCreateNumberFormat(requestedLocale,
                                             internalOptions,
                                             resolved);

  // We can't get information about number or currency style from ICU, so we
  // assume user request was fulfilled.
  if (internalOptions.style === 'currency') {
    Object.defineProperty(resolved, 'currencyDisplay', {value: currencyDisplay,
                                                        writable: true});
  }

  Object.defineProperty(numberFormat, 'formatter', {value: formatter});
  Object.defineProperty(numberFormat, 'resolved', {value: resolved});
  Object.defineProperty(numberFormat, '__initializedIntlObject',
                        {value: 'numberformat'});

  return numberFormat;
}


/**
 * Constructs Intl.NumberFormat object given optional locales and options
 * parameters.
 *
 * @constructor
 */
%SetProperty(Intl, 'NumberFormat', function() {
    var locales = arguments[0];
    var options = arguments[1];

    if (!this || this === Intl) {
      // Constructor is called as a function.
      return new Intl.NumberFormat(locales, options);
    }

    return initializeNumberFormat(toObject(this), locales, options);
  },
  ATTRIBUTES.DONT_ENUM
);


/**
 * NumberFormat resolvedOptions method.
 */
%SetProperty(Intl.NumberFormat.prototype, 'resolvedOptions', function() {
    if (%_IsConstructCall()) {
      throw new TypeError(ORDINARY_FUNCTION_CALLED_AS_CONSTRUCTOR);
    }

    if (!this || typeof this !== 'object' ||
        this.__initializedIntlObject !== 'numberformat') {
      throw new TypeError('resolvedOptions method called on a non-object' +
          ' or on a object that is not Intl.NumberFormat.');
    }

    var format = this;
    var locale = getOptimalLanguageTag(format.resolved.requestedLocale,
                                       format.resolved.locale);

    var result = {
      locale: locale,
      numberingSystem: format.resolved.numberingSystem,
      style: format.resolved.style,
      useGrouping: format.resolved.useGrouping,
      minimumIntegerDigits: format.resolved.minimumIntegerDigits,
      minimumFractionDigits: format.resolved.minimumFractionDigits,
      maximumFractionDigits: format.resolved.maximumFractionDigits,
    };

    if (result.style === 'currency') {
      defineWECProperty(result, 'currency', format.resolved.currency);
      defineWECProperty(result, 'currencyDisplay',
                        format.resolved.currencyDisplay);
    }

    if (format.resolved.hasOwnProperty('minimumSignificantDigits')) {
      defineWECProperty(result, 'minimumSignificantDigits',
                        format.resolved.minimumSignificantDigits);
    }

    if (format.resolved.hasOwnProperty('maximumSignificantDigits')) {
      defineWECProperty(result, 'maximumSignificantDigits',
                        format.resolved.maximumSignificantDigits);
    }

    return result;
  },
  ATTRIBUTES.DONT_ENUM
);
%FunctionSetName(Intl.NumberFormat.prototype.resolvedOptions,
                 'resolvedOptions');
%FunctionRemovePrototype(Intl.NumberFormat.prototype.resolvedOptions);
%SetNativeFlag(Intl.NumberFormat.prototype.resolvedOptions);


/**
 * Returns the subset of the given locale list for which this locale list
 * has a matching (possibly fallback) locale. Locales appear in the same
 * order in the returned list as in the input list.
 * Options are optional parameter.
 */
%SetProperty(Intl.NumberFormat, 'supportedLocalesOf', function(locales) {
    if (%_IsConstructCall()) {
      throw new TypeError(ORDINARY_FUNCTION_CALLED_AS_CONSTRUCTOR);
    }

    return supportedLocalesOf('numberformat', locales, arguments[1]);
  },
  ATTRIBUTES.DONT_ENUM
);
%FunctionSetName(Intl.NumberFormat.supportedLocalesOf, 'supportedLocalesOf');
%FunctionRemovePrototype(Intl.NumberFormat.supportedLocalesOf);
%SetNativeFlag(Intl.NumberFormat.supportedLocalesOf);


/**
 * Returns a String value representing the result of calling ToNumber(value)
 * according to the effective locale and the formatting options of this
 * NumberFormat.
 */
function formatNumber(formatter, value) {
  native function NativeJSInternalNumberFormat();

  // Spec treats -0 and +0 as 0.
  var number = Number(value);
  if (number === -0) {
    number = 0;
  }

  return NativeJSInternalNumberFormat(formatter.formatter, number);
}


/**
 * Returns a Number that represents string value that was passed in.
 */
function parseNumber(formatter, value) {
  native function NativeJSInternalNumberParse();

  return NativeJSInternalNumberParse(formatter.formatter, String(value));
}


addBoundMethod(Intl.NumberFormat, 'format', formatNumber, 1);
addBoundMethod(Intl.NumberFormat, 'v8Parse', parseNumber, 1);
