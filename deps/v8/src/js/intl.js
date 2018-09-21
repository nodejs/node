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
var patternSymbol = utils.ImportNow("intl_pattern_symbol");
var resolvedSymbol = utils.ImportNow("intl_resolved_symbol");
var StringSubstr = GlobalString.prototype.substr;
var StringSubstring = GlobalString.prototype.substring;

utils.Import(function(from) {
  ArrayJoin = from.ArrayJoin;
  ArrayPush = from.ArrayPush;
});

// Utilities for definitions

macro NUMBER_IS_NAN(arg)
(%IS_VAR(arg) !== arg)
endmacro

// To avoid ES2015 Function name inference.

macro ANONYMOUS_FUNCTION(fn)
(0, (fn))
endmacro

/**
 * Adds bound method to the prototype of the given object.
 */
function AddBoundMethod(obj, methodName, implementation, length, type,
                        compat) {
  %CheckIsBootstrapping();
  var internalName = %CreatePrivateSymbol(methodName);

  DEFINE_METHOD(
    obj.prototype,
    get [methodName]() {
      if(!IS_RECEIVER(this)) {
        throw %make_type_error(kIncompatibleMethodReceiver, methodName, this);
      }
      var receiver = %IntlUnwrapReceiver(this, type, obj, methodName, compat);
      if (IS_UNDEFINED(receiver[internalName])) {
        var boundMethod;
        if (IS_UNDEFINED(length) || length === 2) {
          boundMethod =
            ANONYMOUS_FUNCTION((fst, snd) => implementation(receiver, fst, snd));
        } else if (length === 1) {
          boundMethod = ANONYMOUS_FUNCTION(fst => implementation(receiver, fst));
        } else {
          boundMethod = ANONYMOUS_FUNCTION((...args) => {
            // DateTimeFormat.format needs to be 0 arg method, but can still
            // receive an optional dateValue param. If one was provided, pass it
            // along.
            if (args.length > 0) {
              return implementation(receiver, args[0]);
            } else {
              return implementation(receiver);
            }
          });
        }
        %SetNativeFlag(boundMethod);
        receiver[internalName] = boundMethod;
      }
      return receiver[internalName];
    }
  );
}

function IntlConstruct(receiver, constructor, create, newTarget, args,
                       compat) {
  var locales = args[0];
  var options = args[1];

  var instance = create(locales, options);

  if (compat && IS_UNDEFINED(newTarget) && receiver instanceof constructor) {
    %object_define_property(receiver, IntlFallbackSymbol, { value: instance });
    return receiver;
  }

  return instance;
}



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
 * Replace quoted text (single quote, anything but the quote and quote again).
 */
var QUOTED_STRING_RE = UNDEFINED;

function GetQuotedStringRE() {
  if (IS_UNDEFINED(QUOTED_STRING_RE)) {
    QUOTED_STRING_RE = new GlobalRegExp("'[^']+'", 'g');
  }
  return QUOTED_STRING_RE;
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
 * Matches valid IANA time zone names.
 */
var TIMEZONE_NAME_CHECK_RE = UNDEFINED;
var GMT_OFFSET_TIMEZONE_NAME_CHECK_RE = UNDEFINED;

function GetTimezoneNameCheckRE() {
  if (IS_UNDEFINED(TIMEZONE_NAME_CHECK_RE)) {
    TIMEZONE_NAME_CHECK_RE = new GlobalRegExp(
        '^([A-Za-z]+)/([A-Za-z_-]+)((?:\/[A-Za-z_-]+)+)*$');
  }
  return TIMEZONE_NAME_CHECK_RE;
}

function GetGMTOffsetTimezoneNameCheckRE() {
  if (IS_UNDEFINED(GMT_OFFSET_TIMEZONE_NAME_CHECK_RE)) {
     GMT_OFFSET_TIMEZONE_NAME_CHECK_RE = new GlobalRegExp(
         '^(?:ETC/GMT)(?<offset>0|[+-](?:[0-9]|1[0-4]))$');
  }
  return GMT_OFFSET_TIMEZONE_NAME_CHECK_RE;
}

/**
 * Matches valid location parts of IANA time zone names.
 */
var TIMEZONE_NAME_LOCATION_PART_RE = UNDEFINED;

function GetTimezoneNameLocationPartRE() {
  if (IS_UNDEFINED(TIMEZONE_NAME_LOCATION_PART_RE)) {
    TIMEZONE_NAME_LOCATION_PART_RE =
        new GlobalRegExp('^([A-Za-z]+)((?:[_-][A-Za-z]+)+)*$');
  }
  return TIMEZONE_NAME_LOCATION_PART_RE;
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
 * Populates internalOptions object with boolean key-value pairs
 * from extensionMap and options.
 * Returns filtered extension (number and date format constructors use
 * Unicode extensions for passing parameters to ICU).
 * It's used for extension-option pairs only, e.g. kn-normalization, but not
 * for 'sensitivity' since it doesn't have extension equivalent.
 * Extensions like nu and ca don't have options equivalent, so we place
 * undefined in the map.property to denote that.
 */
function setOptions(inOptions, extensionMap, keyValues, getOption, outOptions) {
  var extension = '';

  var updateExtension = function updateExtension(key, value) {
    return '-' + key + '-' + TO_STRING(value);
  }

  var updateProperty = function updateProperty(property, type, value) {
    if (type === 'boolean' && (typeof value === 'string')) {
      value = (value === 'true') ? true : false;
    }

    if (!IS_UNDEFINED(property)) {
      %DefineWEProperty(outOptions, property, value);
    }
  }

  for (var key in keyValues) {
    if (HAS_OWN_PROPERTY(keyValues, key)) {
      var value = UNDEFINED;
      var map = keyValues[key];
      if (!IS_UNDEFINED(map.property)) {
        // This may return true if user specifies numeric: 'false', since
        // Boolean('nonempty') === true.
        value = getOption(map.property, map.type, map.values);
      }
      if (!IS_UNDEFINED(value)) {
        updateProperty(map.property, map.type, value);
        extension += updateExtension(key, value);
        continue;
      }
      // User options didn't have it, check Unicode extension.
      // Here we want to convert strings 'true', 'false' into proper Boolean
      // values (not a user error).
      if (HAS_OWN_PROPERTY(extensionMap, key)) {
        value = extensionMap[key];
        if (!IS_UNDEFINED(value)) {
          updateProperty(map.property, map.type, value);
          extension += updateExtension(key, value);
        } else if (map.type === 'boolean') {
          // Boolean keys are allowed not to have values in Unicode extension.
          // Those default to true.
          updateProperty(map.property, map.type, true);
          extension += updateExtension(key, true);
        }
      }
    }
  }

  return extension === ''? '' : '-u' + extension;
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
 * Defines a property and sets writable, enumerable and configurable to true.
 */
function defineWECProperty(object, property, value) {
  %object_define_property(object, property, {value: value,
                                             writable: true,
                                             enumerable: true,
                                             configurable: true});
}


/**
 * Adds property to an object if the value is not undefined.
 * Sets all descriptors to true.
 */
function addWECPropertyIfDefined(object, property, value) {
  if (!IS_UNDEFINED(value)) {
    defineWECProperty(object, property, value);
  }
}


/**
 * Returns titlecased word, aMeRricA -> America.
 */
function toTitleCaseWord(word) {
  return %StringToUpperCaseIntl(%_Call(StringSubstr, word, 0, 1)) +
         %StringToLowerCaseIntl(%_Call(StringSubstr, word, 1));
}

/**
 * Returns titlecased location, bueNos_airES -> Buenos_Aires
 * or ho_cHi_minH -> Ho_Chi_Minh. It is locale-agnostic and only
 * deals with ASCII only characters.
 * 'of', 'au' and 'es' are special-cased and lowercased.
 */
function toTitleCaseTimezoneLocation(location) {
  var match = %regexp_internal_match(GetTimezoneNameLocationPartRE(), location)
  if (IS_NULL(match)) throw %make_range_error(kExpectedLocation, location);

  var result = toTitleCaseWord(match[1]);
  if (!IS_UNDEFINED(match[2]) && 2 < match.length) {
    // The first character is a separator, '_' or '-'.
    // None of IANA zone names has both '_' and '-'.
    var separator = %_Call(StringSubstring, match[2], 0, 1);
    var parts = %StringSplit(match[2], separator, kMaxUint32);
    for (var i = 1; i < parts.length; i++) {
      var part = parts[i]
      var lowercasedPart = %StringToLowerCaseIntl(part);
      result = result + separator +
          ((lowercasedPart !== 'es' &&
            lowercasedPart !== 'of' && lowercasedPart !== 'au') ?
          toTitleCaseWord(part) : lowercasedPart);
    }
  }
  return result;
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

/**
 * Collator resolvedOptions method.
 */
DEFINE_METHOD(
  GlobalIntlCollator.prototype,
  resolvedOptions() {
    return %CollatorResolvedOptions(this);
  }
);


/**
 * Returns the subset of the given locale list for which this locale list
 * has a matching (possibly fallback) locale. Locales appear in the same
 * order in the returned list as in the input list.
 * Options are optional parameter.
 */
DEFINE_METHOD(
  GlobalIntlCollator,
  supportedLocalesOf(locales) {
    return %SupportedLocalesOf('collator', locales, arguments[1]);
  }
);


DEFINE_METHOD(
  GlobalIntlPluralRules.prototype,
  resolvedOptions() {
    return %PluralRulesResolvedOptions(this);
  }
);

DEFINE_METHOD(
  GlobalIntlPluralRules,
  supportedLocalesOf(locales) {
    return %SupportedLocalesOf('pluralrules', locales, arguments[1]);
  }
);

DEFINE_METHOD(
  GlobalIntlPluralRules.prototype,
  select(value) {
    return %PluralRulesSelect(this, TO_NUMBER(value) + 0);
  }
);

// ECMA 402 #sec-setnfdigitoptions
// SetNumberFormatDigitOptions ( intlObj, options, mnfdDefault, mxfdDefault )
function SetNumberFormatDigitOptions(internalOptions, options,
                                     mnfdDefault, mxfdDefault) {
  // Digit ranges.
  var mnid = %GetNumberOption(options, 'minimumIntegerDigits', 1, 21, 1);
  %DefineWEProperty(internalOptions, 'minimumIntegerDigits', mnid);

  var mnfd = %GetNumberOption(options, 'minimumFractionDigits', 0, 20,
                             mnfdDefault);
  %DefineWEProperty(internalOptions, 'minimumFractionDigits', mnfd);

  var mxfdActualDefault = MathMax(mnfd, mxfdDefault);

  var mxfd = %GetNumberOption(options, 'maximumFractionDigits', mnfd, 20,
                             mxfdActualDefault);

  %DefineWEProperty(internalOptions, 'maximumFractionDigits', mxfd);

  var mnsd = options['minimumSignificantDigits'];
  var mxsd = options['maximumSignificantDigits'];
  if (!IS_UNDEFINED(mnsd) || !IS_UNDEFINED(mxsd)) {
    mnsd = %DefaultNumberOption(mnsd, 1, 21, 1, 'minimumSignificantDigits');
    %DefineWEProperty(internalOptions, 'minimumSignificantDigits', mnsd);

    mxsd = %DefaultNumberOption(mxsd, mnsd, 21, 21, 'maximumSignificantDigits');
    %DefineWEProperty(internalOptions, 'maximumSignificantDigits', mxsd);
  }
}

/**
 * Initializes the given object so it's a valid NumberFormat instance.
 * Useful for subclassing.
 */
function CreateNumberFormat(locales, options) {
  if (IS_UNDEFINED(options)) {
    options = {__proto__: null};
  } else {
    options = TO_OBJECT(options);
  }

  var getOption = getGetOption(options, 'numberformat');

  var locale = resolveLocale('numberformat', locales, options);

  var internalOptions = {__proto__: null};
  %DefineWEProperty(internalOptions, 'style', getOption(
    'style', 'string', ['decimal', 'percent', 'currency'], 'decimal'));

  var currency = getOption('currency', 'string');
  if (!IS_UNDEFINED(currency) && !%IsWellFormedCurrencyCode(currency)) {
    throw %make_range_error(kInvalidCurrencyCode, currency);
  }

  if (internalOptions.style === 'currency' && IS_UNDEFINED(currency)) {
    throw %make_type_error(kCurrencyCode);
  }

  var mnfdDefault, mxfdDefault;

  var currencyDisplay = getOption(
      'currencyDisplay', 'string', ['code', 'symbol', 'name'], 'symbol');
  if (internalOptions.style === 'currency') {
    %DefineWEProperty(internalOptions, 'currency', %StringToUpperCaseIntl(currency));
    %DefineWEProperty(internalOptions, 'currencyDisplay', currencyDisplay);

    mnfdDefault = mxfdDefault = %CurrencyDigits(internalOptions.currency);
  } else {
    mnfdDefault = 0;
    mxfdDefault = internalOptions.style === 'percent' ? 0 : 3;
  }

  SetNumberFormatDigitOptions(internalOptions, options, mnfdDefault,
                              mxfdDefault);

  // Grouping.
  %DefineWEProperty(internalOptions, 'useGrouping', getOption(
    'useGrouping', 'boolean', UNDEFINED, true));

  // ICU prefers options to be passed using -u- extension key/values for
  // number format, so we need to build that.
  var extensionMap = %ParseExtension(locale.extension);

  /**
   * Map of Unicode extensions to option properties, and their values and types,
   * for a number format.
   */
  var NUMBER_FORMAT_KEY_MAP = {
    __proto__: null,
    'nu': {__proto__: null, 'property': UNDEFINED, 'type': 'string'}
  };

  var extension = setOptions(options, extensionMap, NUMBER_FORMAT_KEY_MAP,
                             getOption, internalOptions);

  var requestedLocale = locale.locale + extension;
  var resolved = %object_define_properties({__proto__: null}, {
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
  if (HAS_OWN_PROPERTY(internalOptions, 'minimumSignificantDigits')) {
    %DefineWEProperty(resolved, 'minimumSignificantDigits', UNDEFINED);
  }
  if (HAS_OWN_PROPERTY(internalOptions, 'maximumSignificantDigits')) {
    %DefineWEProperty(resolved, 'maximumSignificantDigits', UNDEFINED);
  }
  var numberFormat = %CreateNumberFormat(requestedLocale, internalOptions,
                                         resolved);

  if (internalOptions.style === 'currency') {
    %object_define_property(resolved, 'currencyDisplay',
        {value: currencyDisplay, writable: true});
  }

  %MarkAsInitializedIntlObjectOfType(numberFormat, NUMBER_FORMAT_TYPE);
  numberFormat[resolvedSymbol] = resolved;

  return numberFormat;
}


/**
 * Constructs Intl.NumberFormat object given optional locales and options
 * parameters.
 *
 * @constructor
 */
function NumberFormatConstructor() {
  return IntlConstruct(this, GlobalIntlNumberFormat, CreateNumberFormat,
                       new.target, arguments, true);
}
%SetCode(GlobalIntlNumberFormat, NumberFormatConstructor);


/**
 * NumberFormat resolvedOptions method.
 */
DEFINE_METHOD(
  GlobalIntlNumberFormat.prototype,
  resolvedOptions() {
    var methodName = 'resolvedOptions';
    if(!IS_RECEIVER(this)) {
      throw %make_type_error(kIncompatibleMethodReceiver, methodName, this);
    }
    var format = %IntlUnwrapReceiver(this, NUMBER_FORMAT_TYPE,
                                     GlobalIntlNumberFormat,
                                     methodName, true);
    var result = {
      locale: format[resolvedSymbol].locale,
      numberingSystem: format[resolvedSymbol].numberingSystem,
      style: format[resolvedSymbol].style,
      useGrouping: format[resolvedSymbol].useGrouping,
      minimumIntegerDigits: format[resolvedSymbol].minimumIntegerDigits,
      minimumFractionDigits: format[resolvedSymbol].minimumFractionDigits,
      maximumFractionDigits: format[resolvedSymbol].maximumFractionDigits,
    };

    if (result.style === 'currency') {
      defineWECProperty(result, 'currency', format[resolvedSymbol].currency);
      defineWECProperty(result, 'currencyDisplay',
                        format[resolvedSymbol].currencyDisplay);
    }

    if (HAS_OWN_PROPERTY(format[resolvedSymbol], 'minimumSignificantDigits')) {
      defineWECProperty(result, 'minimumSignificantDigits',
                        format[resolvedSymbol].minimumSignificantDigits);
    }

    if (HAS_OWN_PROPERTY(format[resolvedSymbol], 'maximumSignificantDigits')) {
      defineWECProperty(result, 'maximumSignificantDigits',
                        format[resolvedSymbol].maximumSignificantDigits);
    }

    return result;
  }
);


/**
 * Returns the subset of the given locale list for which this locale list
 * has a matching (possibly fallback) locale. Locales appear in the same
 * order in the returned list as in the input list.
 * Options are optional parameter.
 */
DEFINE_METHOD(
  GlobalIntlNumberFormat,
  supportedLocalesOf(locales) {
    return %SupportedLocalesOf('numberformat', locales, arguments[1]);
  }
);

/**
 * Returns a string that matches LDML representation of the options object.
 */
function toLDMLString(options) {
  var getOption = getGetOption(options, 'dateformat');

  var ldmlString = '';

  var option = getOption('weekday', 'string', ['narrow', 'short', 'long']);
  ldmlString += appendToLDMLString(
      option, {narrow: 'EEEEE', short: 'EEE', long: 'EEEE'});

  option = getOption('era', 'string', ['narrow', 'short', 'long']);
  ldmlString += appendToLDMLString(
      option, {narrow: 'GGGGG', short: 'GGG', long: 'GGGG'});

  option = getOption('year', 'string', ['2-digit', 'numeric']);
  ldmlString += appendToLDMLString(option, {'2-digit': 'yy', 'numeric': 'y'});

  option = getOption('month', 'string',
                     ['2-digit', 'numeric', 'narrow', 'short', 'long']);
  ldmlString += appendToLDMLString(option, {'2-digit': 'MM', 'numeric': 'M',
          'narrow': 'MMMMM', 'short': 'MMM', 'long': 'MMMM'});

  option = getOption('day', 'string', ['2-digit', 'numeric']);
  ldmlString += appendToLDMLString(
      option, {'2-digit': 'dd', 'numeric': 'd'});

  var hr12 = getOption('hour12', 'boolean');
  option = getOption('hour', 'string', ['2-digit', 'numeric']);
  if (IS_UNDEFINED(hr12)) {
    ldmlString += appendToLDMLString(option, {'2-digit': 'jj', 'numeric': 'j'});
  } else if (hr12 === true) {
    ldmlString += appendToLDMLString(option, {'2-digit': 'hh', 'numeric': 'h'});
  } else {
    ldmlString += appendToLDMLString(option, {'2-digit': 'HH', 'numeric': 'H'});
  }

  option = getOption('minute', 'string', ['2-digit', 'numeric']);
  ldmlString += appendToLDMLString(option, {'2-digit': 'mm', 'numeric': 'm'});

  option = getOption('second', 'string', ['2-digit', 'numeric']);
  ldmlString += appendToLDMLString(option, {'2-digit': 'ss', 'numeric': 's'});

  option = getOption('timeZoneName', 'string', ['short', 'long']);
  ldmlString += appendToLDMLString(option, {short: 'z', long: 'zzzz'});

  return ldmlString;
}


/**
 * Returns either LDML equivalent of the current option or empty string.
 */
function appendToLDMLString(option, pairs) {
  if (!IS_UNDEFINED(option)) {
    return pairs[option];
  } else {
    return '';
  }
}


/**
 * Returns object that matches LDML representation of the date.
 */
function fromLDMLString(ldmlString) {
  // First remove '' quoted text, so we lose 'Uhr' strings.
  ldmlString = %RegExpInternalReplace(GetQuotedStringRE(), ldmlString, '');

  var options = {__proto__: null};
  var match = %regexp_internal_match(/E{3,5}/, ldmlString);
  options = appendToDateTimeObject(
      options, 'weekday', match, {EEEEE: 'narrow', EEE: 'short', EEEE: 'long'});

  match = %regexp_internal_match(/G{3,5}/, ldmlString);
  options = appendToDateTimeObject(
      options, 'era', match, {GGGGG: 'narrow', GGG: 'short', GGGG: 'long'});

  match = %regexp_internal_match(/y{1,2}/, ldmlString);
  options = appendToDateTimeObject(
      options, 'year', match, {y: 'numeric', yy: '2-digit'});

  match = %regexp_internal_match(/M{1,5}/, ldmlString);
  options = appendToDateTimeObject(options, 'month', match, {MM: '2-digit',
      M: 'numeric', MMMMM: 'narrow', MMM: 'short', MMMM: 'long'});

  // Sometimes we get L instead of M for month - standalone name.
  match = %regexp_internal_match(/L{1,5}/, ldmlString);
  options = appendToDateTimeObject(options, 'month', match, {LL: '2-digit',
      L: 'numeric', LLLLL: 'narrow', LLL: 'short', LLLL: 'long'});

  match = %regexp_internal_match(/d{1,2}/, ldmlString);
  options = appendToDateTimeObject(
      options, 'day', match, {d: 'numeric', dd: '2-digit'});

  match = %regexp_internal_match(/h{1,2}/, ldmlString);
  if (match !== null) {
    options['hour12'] = true;
  }
  options = appendToDateTimeObject(
      options, 'hour', match, {h: 'numeric', hh: '2-digit'});

  match = %regexp_internal_match(/H{1,2}/, ldmlString);
  if (match !== null) {
    options['hour12'] = false;
  }
  options = appendToDateTimeObject(
      options, 'hour', match, {H: 'numeric', HH: '2-digit'});

  match = %regexp_internal_match(/m{1,2}/, ldmlString);
  options = appendToDateTimeObject(
      options, 'minute', match, {m: 'numeric', mm: '2-digit'});

  match = %regexp_internal_match(/s{1,2}/, ldmlString);
  options = appendToDateTimeObject(
      options, 'second', match, {s: 'numeric', ss: '2-digit'});

  match = %regexp_internal_match(/z|zzzz/, ldmlString);
  options = appendToDateTimeObject(
      options, 'timeZoneName', match, {z: 'short', zzzz: 'long'});

  return options;
}


function appendToDateTimeObject(options, option, match, pairs) {
  if (IS_NULL(match)) {
    if (!HAS_OWN_PROPERTY(options, option)) {
      %DefineWEProperty(options, option, UNDEFINED);
    }
    return options;
  }

  var property = match[0];
  %DefineWEProperty(options, option, pairs[property]);

  return options;
}


/**
 * Returns options with at least default values in it.
 */
function toDateTimeOptions(options, required, defaults) {
  if (IS_UNDEFINED(options)) {
    options = {__proto__: null};
  } else {
    options = TO_OBJECT(options);
  }

  options = %object_create(options);

  var needsDefault = true;
  if ((required === 'date' || required === 'any') &&
      (!IS_UNDEFINED(options.weekday) || !IS_UNDEFINED(options.year) ||
       !IS_UNDEFINED(options.month) || !IS_UNDEFINED(options.day))) {
    needsDefault = false;
  }

  if ((required === 'time' || required === 'any') &&
      (!IS_UNDEFINED(options.hour) || !IS_UNDEFINED(options.minute) ||
       !IS_UNDEFINED(options.second))) {
    needsDefault = false;
  }

  if (needsDefault && (defaults === 'date' || defaults === 'all')) {
    %object_define_property(options, 'year', {value: 'numeric',
                                              writable: true,
                                              enumerable: true,
                                              configurable: true});
    %object_define_property(options, 'month', {value: 'numeric',
                                               writable: true,
                                               enumerable: true,
                                               configurable: true});
    %object_define_property(options, 'day', {value: 'numeric',
                                             writable: true,
                                             enumerable: true,
                                             configurable: true});
  }

  if (needsDefault && (defaults === 'time' || defaults === 'all')) {
    %object_define_property(options, 'hour', {value: 'numeric',
                                              writable: true,
                                              enumerable: true,
                                              configurable: true});
    %object_define_property(options, 'minute', {value: 'numeric',
                                                writable: true,
                                                enumerable: true,
                                                configurable: true});
    %object_define_property(options, 'second', {value: 'numeric',
                                                writable: true,
                                                enumerable: true,
                                                configurable: true});
  }

  return options;
}


/**
 * Initializes the given object so it's a valid DateTimeFormat instance.
 * Useful for subclassing.
 */
function CreateDateTimeFormat(locales, options) {
  if (IS_UNDEFINED(options)) {
    options = {__proto__: null};
  }

  var locale = resolveLocale('dateformat', locales, options);

  options = %ToDateTimeOptions(options, 'any', 'date');

  var getOption = getGetOption(options, 'dateformat');

  // We implement only best fit algorithm, but still need to check
  // if the formatMatcher values are in range.
  var matcher = getOption('formatMatcher', 'string',
                          ['basic', 'best fit'], 'best fit');

  // Build LDML string for the skeleton that we pass to the formatter.
  var ldmlString = toLDMLString(options);

  // Filter out supported extension keys so we know what to put in resolved
  // section later on.
  // We need to pass calendar and number system to the method.
  var tz = canonicalizeTimeZoneID(options.timeZone);

  // ICU prefers options to be passed using -u- extension key/values, so
  // we need to build that.
  var internalOptions = {__proto__: null};
  var extensionMap = %ParseExtension(locale.extension);

  /**
   * Map of Unicode extensions to option properties, and their values and types,
   * for a date/time format.
   */
  var DATETIME_FORMAT_KEY_MAP = {
    __proto__: null,
    'ca': {__proto__: null, 'property': UNDEFINED, 'type': 'string'},
    'nu': {__proto__: null, 'property': UNDEFINED, 'type': 'string'}
  };

  var extension = setOptions(options, extensionMap, DATETIME_FORMAT_KEY_MAP,
                             getOption, internalOptions);

  var requestedLocale = locale.locale + extension;
  var resolved = %object_define_properties({__proto__: null}, {
    calendar: {writable: true},
    day: {writable: true},
    era: {writable: true},
    hour12: {writable: true},
    hour: {writable: true},
    locale: {writable: true},
    minute: {writable: true},
    month: {writable: true},
    numberingSystem: {writable: true},
    [patternSymbol]: {writable: true},
    requestedLocale: {value: requestedLocale, writable: true},
    second: {writable: true},
    timeZone: {writable: true},
    timeZoneName: {writable: true},
    tz: {value: tz, writable: true},
    weekday: {writable: true},
    year: {writable: true}
  });

  var dateFormat = %CreateDateTimeFormat(
    requestedLocale,
    {__proto__: null, skeleton: ldmlString, timeZone: tz}, resolved);

  if (resolved.timeZone === "Etc/Unknown") {
    throw %make_range_error(kInvalidTimeZone, tz);
  }

  %MarkAsInitializedIntlObjectOfType(dateFormat, DATE_TIME_FORMAT_TYPE);
  dateFormat[resolvedSymbol] = resolved;

  return dateFormat;
}


/**
 * Constructs Intl.DateTimeFormat object given optional locales and options
 * parameters.
 *
 * @constructor
 */
function DateTimeFormatConstructor() {
  return IntlConstruct(this, GlobalIntlDateTimeFormat, CreateDateTimeFormat,
                       new.target, arguments, true);
}
%SetCode(GlobalIntlDateTimeFormat, DateTimeFormatConstructor);


/**
 * DateTimeFormat resolvedOptions method.
 */
DEFINE_METHOD(
  GlobalIntlDateTimeFormat.prototype,
  resolvedOptions() {
    var methodName = 'resolvedOptions';
    if(!IS_RECEIVER(this)) {
      throw %make_type_error(kIncompatibleMethodReceiver, methodName, this);
    }
    var format = %IntlUnwrapReceiver(this, DATE_TIME_FORMAT_TYPE,
                                     GlobalIntlDateTimeFormat,
                                     methodName, true);

    /**
     * Maps ICU calendar names to LDML/BCP47 types for key 'ca'.
     * See typeMap section in third_party/icu/source/data/misc/keyTypeData.txt
     * and
     * http://www.unicode.org/repos/cldr/tags/latest/common/bcp47/calendar.xml
     */
    var ICU_CALENDAR_MAP = {
      __proto__: null,
      'gregorian': 'gregory',
      'ethiopic-amete-alem': 'ethioaa'
    };

    var fromPattern = fromLDMLString(format[resolvedSymbol][patternSymbol]);
    var userCalendar = ICU_CALENDAR_MAP[format[resolvedSymbol].calendar];
    if (IS_UNDEFINED(userCalendar)) {
      // No match means that ICU's legacy name is identical to LDML/BCP type.
      userCalendar = format[resolvedSymbol].calendar;
    }

    var result = {
      locale: format[resolvedSymbol].locale,
      numberingSystem: format[resolvedSymbol].numberingSystem,
      calendar: userCalendar,
      timeZone: format[resolvedSymbol].timeZone
    };

    addWECPropertyIfDefined(result, 'timeZoneName', fromPattern.timeZoneName);
    addWECPropertyIfDefined(result, 'era', fromPattern.era);
    addWECPropertyIfDefined(result, 'year', fromPattern.year);
    addWECPropertyIfDefined(result, 'month', fromPattern.month);
    addWECPropertyIfDefined(result, 'day', fromPattern.day);
    addWECPropertyIfDefined(result, 'weekday', fromPattern.weekday);
    addWECPropertyIfDefined(result, 'hour12', fromPattern.hour12);
    addWECPropertyIfDefined(result, 'hour', fromPattern.hour);
    addWECPropertyIfDefined(result, 'minute', fromPattern.minute);
    addWECPropertyIfDefined(result, 'second', fromPattern.second);

    return result;
  }
);


/**
 * Returns the subset of the given locale list for which this locale list
 * has a matching (possibly fallback) locale. Locales appear in the same
 * order in the returned list as in the input list.
 * Options are optional parameter.
 */
DEFINE_METHOD(
  GlobalIntlDateTimeFormat,
  supportedLocalesOf(locales) {
    return %SupportedLocalesOf('dateformat', locales, arguments[1]);
  }
);


/**
 * Returns canonical Area/Location(/Location) name, or throws an exception
 * if the zone name is invalid IANA name.
 */
function canonicalizeTimeZoneID(tzID) {
  // Skip undefined zones.
  if (IS_UNDEFINED(tzID)) {
    return tzID;
  }

  // Convert zone name to string.
  tzID = TO_STRING(tzID);

  // Special case handling (UTC, GMT).
  var upperID = %StringToUpperCaseIntl(tzID);
  if (upperID === 'UTC' || upperID === 'GMT' ||
      upperID === 'ETC/UTC' || upperID === 'ETC/GMT') {
    return 'UTC';
  }

  // We expect only _, '-' and / beside ASCII letters.
  // All inputs should conform to Area/Location(/Location)*, or Etc/GMT* .
  // TODO(jshin): 1. Support 'GB-Eire", 'EST5EDT", "ROK', 'US/*', 'NZ' and many
  // other aliases/linked names when moving timezone validation code to C++.
  // See crbug.com/364374 and crbug.com/v8/8007 .
  // 2. Resolve the difference betwee CLDR/ICU and IANA time zone db.
  // See http://unicode.org/cldr/trac/ticket/9892 and crbug.com/645807 .
  let match = %regexp_internal_match(GetTimezoneNameCheckRE(), tzID);
  if (IS_NULL(match)) {
    let match =
      %regexp_internal_match(GetGMTOffsetTimezoneNameCheckRE(), upperID);
     if (!IS_NULL(match) && match.length == 2)
       return "Etc/GMT" + match.groups.offset;
     else
       throw %make_range_error(kInvalidTimeZone, tzID);
  }

  let result = toTitleCaseTimezoneLocation(match[1]) + '/' +
               toTitleCaseTimezoneLocation(match[2]);

  if (!IS_UNDEFINED(match[3]) && 3 < match.length) {
    let locations = %StringSplit(match[3], '/', kMaxUint32);
    // The 1st element is empty. Starts with i=1.
    for (var i = 1; i < locations.length; i++) {
      result = result + '/' + toTitleCaseTimezoneLocation(locations[i]);
    }
  }

  return result;
}

/**
 * Initializes the given object so it's a valid BreakIterator instance.
 * Useful for subclassing.
 */
function CreateBreakIterator(locales, options) {
  if (IS_UNDEFINED(options)) {
    options = {__proto__: null};
  }

  var getOption = getGetOption(options, 'breakiterator');

  var internalOptions = {__proto__: null};

  %DefineWEProperty(internalOptions, 'type', getOption(
    'type', 'string', ['character', 'word', 'sentence', 'line'], 'word'));

  var locale = resolveLocale('breakiterator', locales, options);
  var resolved = %object_define_properties({__proto__: null}, {
    requestedLocale: {value: locale.locale, writable: true},
    type: {value: internalOptions.type, writable: true},
    locale: {writable: true}
  });

  var iterator = %CreateBreakIterator(locale.locale, internalOptions, resolved);

  %MarkAsInitializedIntlObjectOfType(iterator, BREAK_ITERATOR_TYPE);
  iterator[resolvedSymbol] = resolved;

  return iterator;
}


/**
 * Constructs Intl.v8BreakIterator object given optional locales and options
 * parameters.
 *
 * @constructor
 */
function v8BreakIteratorConstructor() {
  return IntlConstruct(this, GlobalIntlv8BreakIterator, CreateBreakIterator,
                       new.target, arguments);
}
%SetCode(GlobalIntlv8BreakIterator, v8BreakIteratorConstructor);


/**
 * BreakIterator resolvedOptions method.
 */
DEFINE_METHOD(
  GlobalIntlv8BreakIterator.prototype,
  resolvedOptions() {
    if (!IS_UNDEFINED(new.target)) {
      throw %make_type_error(kOrdinaryFunctionCalledAsConstructor);
    }

    var methodName = 'resolvedOptions';
    if(!IS_RECEIVER(this)) {
      throw %make_type_error(kIncompatibleMethodReceiver, methodName, this);
    }
    var segmenter = %IntlUnwrapReceiver(this, BREAK_ITERATOR_TYPE,
                                        GlobalIntlv8BreakIterator, methodName,
                                        false);

    return {
      locale: segmenter[resolvedSymbol].locale,
      type: segmenter[resolvedSymbol].type
    };
  }
);


/**
 * Returns the subset of the given locale list for which this locale list
 * has a matching (possibly fallback) locale. Locales appear in the same
 * order in the returned list as in the input list.
 * Options are optional parameter.
 */
DEFINE_METHOD(
  GlobalIntlv8BreakIterator,
  supportedLocalesOf(locales) {
    if (!IS_UNDEFINED(new.target)) {
      throw %make_type_error(kOrdinaryFunctionCalledAsConstructor);
    }

    return %SupportedLocalesOf('breakiterator', locales, arguments[1]);
  }
);


/**
 * Returns index of the first break in the string and moves current pointer.
 */
function first(iterator) {
  return %BreakIteratorFirst(iterator);
}


/**
 * Returns the index of the next break and moves the pointer.
 */
function next(iterator) {
  return %BreakIteratorNext(iterator);
}


/**
 * Returns index of the current break.
 */
function current(iterator) {
  return %BreakIteratorCurrent(iterator);
}


/**
 * Returns type of the current break.
 */
function breakType(iterator) {
  return %BreakIteratorBreakType(iterator);
}


AddBoundMethod(GlobalIntlv8BreakIterator, 'first', first, 0,
               BREAK_ITERATOR_TYPE, false);
AddBoundMethod(GlobalIntlv8BreakIterator, 'next', next, 0,
               BREAK_ITERATOR_TYPE, false);
AddBoundMethod(GlobalIntlv8BreakIterator, 'current', current, 0,
               BREAK_ITERATOR_TYPE, false);
AddBoundMethod(GlobalIntlv8BreakIterator, 'breakType', breakType, 0,
               BREAK_ITERATOR_TYPE, false);

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

/**
 * Formats a Date object (this) using locale and options values.
 * If locale or options are omitted, defaults are used - both date and time are
 * present in the output.
 */
DEFINE_METHOD(
  GlobalDate.prototype,
  toLocaleString() {
    var locales = arguments[0];
    var options = arguments[1];
    return %ToLocaleDateTime(
        this, locales, options, 'any', 'all', 'dateformatall');
  }
);


/**
 * Formats a Date object (this) using locale and options values.
 * If locale or options are omitted, defaults are used - only date is present
 * in the output.
 */
DEFINE_METHOD(
  GlobalDate.prototype,
  toLocaleDateString() {
    var locales = arguments[0];
    var options = arguments[1];
    return %ToLocaleDateTime(
        this, locales, options, 'date', 'date', 'dateformatdate');
  }
);


/**
 * Formats a Date object (this) using locale and options values.
 * If locale or options are omitted, defaults are used - only time is present
 * in the output.
 */
DEFINE_METHOD(
  GlobalDate.prototype,
  toLocaleTimeString() {
    var locales = arguments[0];
    var options = arguments[1];
    return %ToLocaleDateTime(
        this, locales, options, 'time', 'time', 'dateformattime');
  }
);

})
