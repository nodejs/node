// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ECMAScript 402 API implementation.

/**
 * Intl object is a single object that has some named properties,
 * all of which are constructors.
 */
(function(global, utils) {

"use strict";

%CheckIsBootstrapping();

// -------------------------------------------------------------------
// Imports

var ArrayJoin;
var ArrayPush;
var GlobalDate = global.Date;
var GlobalIntl = global.Intl;
var GlobalIntlDateTimeFormat = GlobalIntl.DateTimeFormat;
var GlobalIntlNumberFormat = GlobalIntl.NumberFormat;
var GlobalIntlCollator = GlobalIntl.Collator;
var GlobalIntlPluralRules = GlobalIntl.PluralRules;
var GlobalIntlv8BreakIterator = GlobalIntl.v8BreakIterator;
var GlobalRegExp = global.RegExp;
var GlobalString = global.String;
var GlobalArray = global.Array;
var IntlFallbackSymbol = utils.ImportNow("intl_fallback_symbol");
var InternalArray = utils.InternalArray;
var MathMax = global.Math.max;
var ObjectHasOwnProperty = global.Object.prototype.hasOwnProperty;
var ObjectKeys = global.Object.keys;
var resolvedSymbol = utils.ImportNow("intl_resolved_symbol");
var StringSubstr = GlobalString.prototype.substr;
var StringSubstring = GlobalString.prototype.substring;

utils.Import(function(from) {
  ArrayJoin = from.ArrayJoin;
  ArrayPush = from.ArrayPush;
});

// -------------------------------------------------------------------

/**
 * Caches available locales for each service.
 */
var AVAILABLE_LOCALES = {
   __proto__ : null,
  'collator': UNDEFINED,
  'numberformat': UNDEFINED,
  'dateformat': UNDEFINED,
  'breakiterator': UNDEFINED,
  'pluralrules': UNDEFINED,
  'relativetimeformat': UNDEFINED,
  'listformat': UNDEFINED,
  'segmenter': UNDEFINED,
};

/**
 * Unicode extension regular expression.
 */
var UNICODE_EXTENSION_RE = UNDEFINED;

function GetUnicodeExtensionRE() {
  if (IS_UNDEFINED(UNDEFINED)) {
    UNICODE_EXTENSION_RE = new GlobalRegExp('-u(-[a-z0-9]{2,8})+', 'g');
  }
  return UNICODE_EXTENSION_RE;
}

/**
 * Matches any Unicode extension.
 */
var ANY_EXTENSION_RE = UNDEFINED;

function GetAnyExtensionRE() {
  if (IS_UNDEFINED(ANY_EXTENSION_RE)) {
    ANY_EXTENSION_RE = new GlobalRegExp('-[a-z0-9]{1}-.*', 'g');
  }
  return ANY_EXTENSION_RE;
}

/**
 * Matches valid service name.
 */
var SERVICE_RE = UNDEFINED;

function GetServiceRE() {
  if (IS_UNDEFINED(SERVICE_RE)) {
    SERVICE_RE =
        new GlobalRegExp('^(' + %_Call(ArrayJoin, ObjectKeys(AVAILABLE_LOCALES), '|') + ')$');
  }
  return SERVICE_RE;
}

/**
 * Returns a getOption function that extracts property value for given
 * options object. If property is missing it returns defaultValue. If value
 * is out of range for that property it throws RangeError.
 */
function getGetOption(options, caller) {
  if (IS_UNDEFINED(options)) throw %make_error(kDefaultOptionsMissing, caller);

  // Ecma 402 #sec-getoption
  var getOption = function (property, type, values, fallback) {
    // 1. Let value be ? Get(options, property).
    var value = options[property];
    // 2. If value is not undefined, then
    if (!IS_UNDEFINED(value)) {
      switch (type) {
        // If type is "boolean", then let value be ToBoolean(value).
        case 'boolean':
          value = TO_BOOLEAN(value);
          break;
        // If type is "string", then let value be ToString(value).
        case 'string':
          value = TO_STRING(value);
          break;
        // Assert: type is "boolean" or "string".
        default:
          throw %make_error(kWrongValueType);
      }

      // d. If values is not undefined, then
      // If values does not contain an element equal to value, throw a
      // RangeError exception.
      if (!IS_UNDEFINED(values) && %ArrayIndexOf(values, value, 0) === -1) {
        throw %make_range_error(kValueOutOfRange, value, caller, property);
      }

      return value;
    }

    return fallback;
  }

  return getOption;
}


/**
 * Ecma 402 9.2.5
 * TODO(jshin): relevantExtensionKeys and localeData need to be taken into
 * account per spec.
 * Compares a BCP 47 language priority list requestedLocales against the locales
 * in availableLocales and determines the best available language to meet the
 * request. Two algorithms are available to match the locales: the Lookup
 * algorithm described in RFC 4647 section 3.4, and an implementation dependent
 * best-fit algorithm. Independent of the locale matching algorithm, options
 * specified through Unicode locale extension sequences are negotiated
 * separately, taking the caller's relevant extension keys and locale data as
 * well as client-provided options into consideration. Returns an object with
 * a locale property whose value is the language tag of the selected locale,
 * and properties for each key in relevantExtensionKeys providing the selected
 * value for that key.
 */
function resolveLocale(service, requestedLocales, options) {
  requestedLocales = initializeLocaleList(requestedLocales);

  var getOption = getGetOption(options, service);
  var matcher = getOption('localeMatcher', 'string',
                          ['lookup', 'best fit'], 'best fit');
  var resolved;
  if (matcher === 'lookup') {
    resolved = lookupMatcher(service, requestedLocales);
  } else {
    resolved = bestFitMatcher(service, requestedLocales);
  }

  return resolved;
}

%InstallToContext([
  "resolve_locale", resolveLocale
]);

/**
 * Look up the longest non-empty prefix of |locale| that is an element of
 * |availableLocales|. Returns undefined when the |locale| is completely
 * unsupported by |availableLocales|.
 */
function bestAvailableLocale(availableLocales, locale) {
  do {
    if (!IS_UNDEFINED(availableLocales[locale])) {
      return locale;
    }
    // Truncate locale if possible.
    var pos = %StringLastIndexOf(locale, '-');
    if (pos === -1) {
      break;
    }
    locale = %_Call(StringSubstring, locale, 0, pos);
  } while (true);

  return UNDEFINED;
}


/**
 * Try to match any mutation of |requestedLocale| against |availableLocales|.
 */
function attemptSingleLookup(availableLocales, requestedLocale) {
  // Remove all extensions.
  var noExtensionsLocale = %RegExpInternalReplace(
      GetAnyExtensionRE(), requestedLocale, '');
  var availableLocale = bestAvailableLocale(
      availableLocales, requestedLocale);
  if (!IS_UNDEFINED(availableLocale)) {
    // Return the resolved locale and extension.
    var extensionMatch = %regexp_internal_match(
        GetUnicodeExtensionRE(), requestedLocale);
    var extension = IS_NULL(extensionMatch) ? '' : extensionMatch[0];
    return {
      __proto__: null,
      locale: availableLocale,
      extension: extension,
      localeWithExtension: availableLocale + extension,
    };
  }
  return UNDEFINED;
}


/**
 * Returns best matched supported locale and extension info using basic
 * lookup algorithm.
 */
function lookupMatcher(service, requestedLocales) {
  if (IS_NULL(%regexp_internal_match(GetServiceRE(), service))) {
    throw %make_error(kWrongServiceType, service);
  }

  var availableLocales = getAvailableLocalesOf(service);

  for (var i = 0; i < requestedLocales.length; ++i) {
    var result = attemptSingleLookup(availableLocales, requestedLocales[i]);
    if (!IS_UNDEFINED(result)) {
      return result;
    }
  }

  var defLocale = %GetDefaultICULocale();

  // While ECMA-402 returns defLocale directly, we have to check if it is
  // supported, as such support is not guaranteed.
  var result = attemptSingleLookup(availableLocales, defLocale);
  if (!IS_UNDEFINED(result)) {
    return result;
  }

  // Didn't find a match, return default.
  return {
    __proto__: null,
    locale: 'und',
    extension: '',
    localeWithExtension: 'und',
  };
}


/**
 * Returns best matched supported locale and extension info using
 * implementation dependend algorithm.
 */
function bestFitMatcher(service, requestedLocales) {
  // TODO(cira): implement better best fit algorithm.
  return lookupMatcher(service, requestedLocales);
}

/**
 * Given an array-like, outputs an Array with the numbered
 * properties copied over and defined
 * configurable: false, writable: false, enumerable: true.
 * When |expandable| is true, the result array can be expanded.
 */
function freezeArray(input) {
  var array = [];
  var l = input.length;
  for (var i = 0; i < l; i++) {
    if (i in input) {
      %object_define_property(array, i, {value: input[i],
                                         configurable: false,
                                         writable: false,
                                         enumerable: true});
    }
  }

  %object_define_property(array, 'length', {value: l, writable: false});
  return array;
}

/* Make JS array[] out of InternalArray */
function makeArray(input) {
  var array = [];
  %MoveArrayContents(input, array);
  return array;
}

/**
 * Returns an Object that contains all of supported locales for a given
 * service.
 * In addition to the supported locales we add xx-ZZ locale for each xx-Yyyy-ZZ
 * that is supported. This is required by the spec.
 */
function getAvailableLocalesOf(service) {
  // Cache these, they don't ever change per service.
  if (!IS_UNDEFINED(AVAILABLE_LOCALES[service])) {
    return AVAILABLE_LOCALES[service];
  }

  var available = %AvailableLocalesOf(service);

  for (var i in available) {
    if (HAS_OWN_PROPERTY(available, i)) {
      var parts = %regexp_internal_match(
          /^([a-z]{2,3})-([A-Z][a-z]{3})-([A-Z]{2})$/, i);
      if (!IS_NULL(parts)) {
        // Build xx-ZZ. We don't care about the actual value,
        // as long it's not undefined.
        available[parts[1] + '-' + parts[3]] = null;
      }
    }
  }

  AVAILABLE_LOCALES[service] = available;

  return available;
}

/**
 * Returns an InternalArray where all locales are canonicalized and duplicates
 * removed.
 * Throws on locales that are not well formed BCP47 tags.
 * ECMA 402 8.2.1 steps 1 (ECMA 402 9.2.1) and 2.
 */
function canonicalizeLocaleList(locales) {
  var seen = new InternalArray();
  if (!IS_UNDEFINED(locales)) {
    // We allow single string localeID.
    if (typeof locales === 'string') {
      %_Call(ArrayPush, seen, %CanonicalizeLanguageTag(locales));
      return seen;
    }

    var o = TO_OBJECT(locales);
    var len = TO_LENGTH(o.length);

    for (var k = 0; k < len; k++) {
      if (k in o) {
        var value = o[k];

        var tag = %CanonicalizeLanguageTag(value);

        if (%ArrayIndexOf(seen, tag, 0) === -1) {
          %_Call(ArrayPush, seen, tag);
        }
      }
    }
  }

  return seen;
}

// TODO(ftang): remove the %InstallToContext once
// initializeLocaleList is available in C++
// https://bugs.chromium.org/p/v8/issues/detail?id=7987
%InstallToContext([
  "canonicalize_locale_list", canonicalizeLocaleList
]);


function initializeLocaleList(locales) {
  return freezeArray(canonicalizeLocaleList(locales));
}

// ECMA 402 section 8.2.1
DEFINE_METHOD(
  GlobalIntl,
  getCanonicalLocales(locales) {
    return makeArray(canonicalizeLocaleList(locales));
  }
);

// Save references to Intl objects and methods we use, for added security.
var savedObjects = {
  __proto__: null,
  'collator': GlobalIntlCollator,
  'numberformat': GlobalIntlNumberFormat,
  'dateformatall': GlobalIntlDateTimeFormat,
  'dateformatdate': GlobalIntlDateTimeFormat,
  'dateformattime': GlobalIntlDateTimeFormat
};


// Default (created with undefined locales and options parameters) collator,
// number and date format instances. They'll be created as needed.
var defaultObjects = {
  __proto__: null,
  'collator': UNDEFINED,
  'numberformat': UNDEFINED,
  'dateformatall': UNDEFINED,
  'dateformatdate': UNDEFINED,
  'dateformattime': UNDEFINED,
};

function clearDefaultObjects() {
  defaultObjects['dateformatall'] = UNDEFINED;
  defaultObjects['dateformatdate'] = UNDEFINED;
  defaultObjects['dateformattime'] = UNDEFINED;
}

var date_cache_version = 0;

function checkDateCacheCurrent() {
  var new_date_cache_version = %DateCacheVersion();
  if (new_date_cache_version == date_cache_version) {
    return;
  }
  date_cache_version = new_date_cache_version;

  clearDefaultObjects();
}

/**
 * Returns cached or newly created instance of a given service.
 * We cache only default instances (where no locales or options are provided).
 */
function cachedOrNewService(service, locales, options, defaults) {
  var useOptions = (IS_UNDEFINED(defaults)) ? options : defaults;
  if (IS_UNDEFINED(locales) && IS_UNDEFINED(options)) {
    checkDateCacheCurrent();
    if (IS_UNDEFINED(defaultObjects[service])) {
      defaultObjects[service] = new savedObjects[service](locales, useOptions);
    }
    return defaultObjects[service];
  }
  return new savedObjects[service](locales, useOptions);
}

// TODO(ftang) remove the %InstallToContext once
// cachedOrNewService is available in C++
%InstallToContext([
  "cached_or_new_service", cachedOrNewService
]);

})
