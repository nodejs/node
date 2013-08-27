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
 * Initializes the given object so it's a valid Collator instance.
 * Useful for subclassing.
 */
function initializeCollator(collator, locales, options) {
  if (collator.hasOwnProperty('__initializedIntlObject')) {
    throw new TypeError('Trying to re-initialize Collator object.');
  }

  if (options === undefined) {
    options = {};
  }

  var getOption = getGetOption(options, 'collator');

  var internalOptions = {};

  defineWEProperty(internalOptions, 'usage', getOption(
    'usage', 'string', ['sort', 'search'], 'sort'));

  var sensitivity = getOption('sensitivity', 'string',
                              ['base', 'accent', 'case', 'variant']);
  if (sensitivity === undefined && internalOptions.usage === 'sort') {
    sensitivity = 'variant';
  }
  defineWEProperty(internalOptions, 'sensitivity', sensitivity);

  defineWEProperty(internalOptions, 'ignorePunctuation', getOption(
    'ignorePunctuation', 'boolean', undefined, false));

  var locale = resolveLocale('collator', locales, options);

  // ICU can't take kb, kc... parameters through localeID, so we need to pass
  // them as options.
  // One exception is -co- which has to be part of the extension, but only for
  // usage: sort, and its value can't be 'standard' or 'search'.
  var extensionMap = parseExtension(locale.extension);
  setOptions(
      options, extensionMap, COLLATOR_KEY_MAP, getOption, internalOptions);

  var collation = 'default';
  var extension = '';
  if (extensionMap.hasOwnProperty('co') && internalOptions.usage === 'sort') {
    if (ALLOWED_CO_VALUES.indexOf(extensionMap.co) !== -1) {
      extension = '-u-co-' + extensionMap.co;
      // ICU can't tell us what the collation is, so save user's input.
      collation = extensionMap.co;
    }
  } else if (internalOptions.usage === 'search') {
    extension = '-u-co-search';
  }
  defineWEProperty(internalOptions, 'collation', collation);

  var requestedLocale = locale.locale + extension;

  // We define all properties C++ code may produce, to prevent security
  // problems. If malicious user decides to redefine Object.prototype.locale
  // we can't just use plain x.locale = 'us' or in C++ Set("locale", "us").
  // Object.defineProperties will either succeed defining or throw an error.
  var resolved = Object.defineProperties({}, {
    caseFirst: {writable: true},
    collation: {value: internalOptions.collation, writable: true},
    ignorePunctuation: {writable: true},
    locale: {writable: true},
    numeric: {writable: true},
    requestedLocale: {value: requestedLocale, writable: true},
    sensitivity: {writable: true},
    strength: {writable: true},
    usage: {value: internalOptions.usage, writable: true}
  });

  var internalCollator = %CreateCollator(requestedLocale,
                                         internalOptions,
                                         resolved);

  // Writable, configurable and enumerable are set to false by default.
  Object.defineProperty(collator, 'collator', {value: internalCollator});
  Object.defineProperty(collator, '__initializedIntlObject',
                        {value: 'collator'});
  Object.defineProperty(collator, 'resolved', {value: resolved});

  return collator;
}


/**
 * Constructs Intl.Collator object given optional locales and options
 * parameters.
 *
 * @constructor
 */
%SetProperty(Intl, 'Collator', function() {
    var locales = arguments[0];
    var options = arguments[1];

    if (!this || this === Intl) {
      // Constructor is called as a function.
      return new Intl.Collator(locales, options);
    }

    return initializeCollator(toObject(this), locales, options);
  },
  ATTRIBUTES.DONT_ENUM
);


/**
 * Collator resolvedOptions method.
 */
%SetProperty(Intl.Collator.prototype, 'resolvedOptions', function() {
    if (%_IsConstructCall()) {
      throw new TypeError(ORDINARY_FUNCTION_CALLED_AS_CONSTRUCTOR);
    }

    if (!this || typeof this !== 'object' ||
        this.__initializedIntlObject !== 'collator') {
      throw new TypeError('resolvedOptions method called on a non-object ' +
                          'or on a object that is not Intl.Collator.');
    }

    var coll = this;
    var locale = getOptimalLanguageTag(coll.resolved.requestedLocale,
                                       coll.resolved.locale);

    return {
      locale: locale,
      usage: coll.resolved.usage,
      sensitivity: coll.resolved.sensitivity,
      ignorePunctuation: coll.resolved.ignorePunctuation,
      numeric: coll.resolved.numeric,
      caseFirst: coll.resolved.caseFirst,
      collation: coll.resolved.collation
    };
  },
  ATTRIBUTES.DONT_ENUM
);
%FunctionSetName(Intl.Collator.prototype.resolvedOptions, 'resolvedOptions');
%FunctionRemovePrototype(Intl.Collator.prototype.resolvedOptions);
%SetNativeFlag(Intl.Collator.prototype.resolvedOptions);


/**
 * Returns the subset of the given locale list for which this locale list
 * has a matching (possibly fallback) locale. Locales appear in the same
 * order in the returned list as in the input list.
 * Options are optional parameter.
 */
%SetProperty(Intl.Collator, 'supportedLocalesOf', function(locales) {
    if (%_IsConstructCall()) {
      throw new TypeError(ORDINARY_FUNCTION_CALLED_AS_CONSTRUCTOR);
    }

    return supportedLocalesOf('collator', locales, arguments[1]);
  },
  ATTRIBUTES.DONT_ENUM
);
%FunctionSetName(Intl.Collator.supportedLocalesOf, 'supportedLocalesOf');
%FunctionRemovePrototype(Intl.Collator.supportedLocalesOf);
%SetNativeFlag(Intl.Collator.supportedLocalesOf);


/**
 * When the compare method is called with two arguments x and y, it returns a
 * Number other than NaN that represents the result of a locale-sensitive
 * String comparison of x with y.
 * The result is intended to order String values in the sort order specified
 * by the effective locale and collation options computed during construction
 * of this Collator object, and will be negative, zero, or positive, depending
 * on whether x comes before y in the sort order, the Strings are equal under
 * the sort order, or x comes after y in the sort order, respectively.
 */
function compare(collator, x, y) {
  return %InternalCompare(collator.collator, String(x), String(y));
};


addBoundMethod(Intl.Collator, 'compare', compare, 2);
