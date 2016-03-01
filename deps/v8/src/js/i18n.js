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

var ArrayIndexOf;
var ArrayJoin;
var ArrayPush;
var IsFinite;
var IsNaN;
var GlobalBoolean = global.Boolean;
var GlobalDate = global.Date;
var GlobalNumber = global.Number;
var GlobalRegExp = global.RegExp;
var GlobalString = global.String;
var MakeError;
var MakeRangeError;
var MakeTypeError;
var MathFloor;
var ObjectDefineProperties = utils.ImportNow("ObjectDefineProperties");
var ObjectDefineProperty = utils.ImportNow("ObjectDefineProperty");
var patternSymbol = utils.ImportNow("intl_pattern_symbol");
var RegExpTest;
var resolvedSymbol = utils.ImportNow("intl_resolved_symbol");
var StringIndexOf;
var StringLastIndexOf;
var StringMatch;
var StringReplace;
var StringSplit;
var StringSubstr;
var StringSubstring;

utils.Import(function(from) {
  ArrayIndexOf = from.ArrayIndexOf;
  ArrayJoin = from.ArrayJoin;
  ArrayPush = from.ArrayPush;
  IsFinite = from.IsFinite;
  IsNaN = from.IsNaN;
  MakeError = from.MakeError;
  MakeRangeError = from.MakeRangeError;
  MakeTypeError = from.MakeTypeError;
  MathFloor = from.MathFloor;
  RegExpTest = from.RegExpTest;
  StringIndexOf = from.StringIndexOf;
  StringLastIndexOf = from.StringLastIndexOf;
  StringMatch = from.StringMatch;
  StringReplace = from.StringReplace;
  StringSplit = from.StringSplit;
  StringSubstr = from.StringSubstr;
  StringSubstring = from.StringSubstring;
});

// -------------------------------------------------------------------

var Intl = {};

%AddNamedProperty(global, "Intl", Intl, DONT_ENUM);

/**
 * Caches available locales for each service.
 */
var AVAILABLE_LOCALES = {
  'collator': UNDEFINED,
  'numberformat': UNDEFINED,
  'dateformat': UNDEFINED,
  'breakiterator': UNDEFINED
};

/**
 * Caches default ICU locale.
 */
var DEFAULT_ICU_LOCALE = UNDEFINED;

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
        new GlobalRegExp('^(collator|numberformat|dateformat|breakiterator)$');
  }
  return SERVICE_RE;
}

/**
 * Validates a language tag against bcp47 spec.
 * Actual value is assigned on first run.
 */
var LANGUAGE_TAG_RE = UNDEFINED;

function GetLanguageTagRE() {
  if (IS_UNDEFINED(LANGUAGE_TAG_RE)) {
    BuildLanguageTagREs();
  }
  return LANGUAGE_TAG_RE;
}

/**
 * Helps find duplicate variants in the language tag.
 */
var LANGUAGE_VARIANT_RE = UNDEFINED;

function GetLanguageVariantRE() {
  if (IS_UNDEFINED(LANGUAGE_VARIANT_RE)) {
    BuildLanguageTagREs();
  }
  return LANGUAGE_VARIANT_RE;
}

/**
 * Helps find duplicate singletons in the language tag.
 */
var LANGUAGE_SINGLETON_RE = UNDEFINED;

function GetLanguageSingletonRE() {
  if (IS_UNDEFINED(LANGUAGE_SINGLETON_RE)) {
    BuildLanguageTagREs();
  }
  return LANGUAGE_SINGLETON_RE;
}

/**
 * Matches valid IANA time zone names.
 */
var TIMEZONE_NAME_CHECK_RE = UNDEFINED;

function GetTimezoneNameCheckRE() {
  if (IS_UNDEFINED(TIMEZONE_NAME_CHECK_RE)) {
    TIMEZONE_NAME_CHECK_RE = new GlobalRegExp(
        '^([A-Za-z]+)/([A-Za-z_-]+)((?:\/[A-Za-z_-]+)+)*$');
  }
  return TIMEZONE_NAME_CHECK_RE;
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
 * Adds bound method to the prototype of the given object.
 */
function addBoundMethod(obj, methodName, implementation, length) {
  %CheckIsBootstrapping();
  function getter() {
    if (!%IsInitializedIntlObject(this)) {
      throw MakeTypeError(kMethodCalledOnWrongObject, methodName);
    }
    var internalName = '__bound' + methodName + '__';
    if (IS_UNDEFINED(this[internalName])) {
      var that = this;
      var boundMethod;
      if (IS_UNDEFINED(length) || length === 2) {
        boundMethod = function(x, y) {
          if (!IS_UNDEFINED(new.target)) {
            throw MakeTypeError(kOrdinaryFunctionCalledAsConstructor);
          }
          return implementation(that, x, y);
        }
      } else if (length === 1) {
        boundMethod = function(x) {
          if (!IS_UNDEFINED(new.target)) {
            throw MakeTypeError(kOrdinaryFunctionCalledAsConstructor);
          }
          return implementation(that, x);
        }
      } else {
        boundMethod = function() {
          if (!IS_UNDEFINED(new.target)) {
            throw MakeTypeError(kOrdinaryFunctionCalledAsConstructor);
          }
          // DateTimeFormat.format needs to be 0 arg method, but can stil
          // receive optional dateValue param. If one was provided, pass it
          // along.
          if (%_ArgumentsLength() > 0) {
            return implementation(that, %_Arguments(0));
          } else {
            return implementation(that);
          }
        }
      }
      %FunctionSetName(boundMethod, internalName);
      %FunctionRemovePrototype(boundMethod);
      %SetNativeFlag(boundMethod);
      this[internalName] = boundMethod;
    }
    return this[internalName];
  }

  %FunctionSetName(getter, methodName);
  %FunctionRemovePrototype(getter);
  %SetNativeFlag(getter);

  ObjectDefineProperty(obj.prototype, methodName, {
    get: getter,
    enumerable: false,
    configurable: true
  });
}


/**
 * Returns an intersection of locales and service supported locales.
 * Parameter locales is treated as a priority list.
 */
function supportedLocalesOf(service, locales, options) {
  if (IS_NULL(%_Call(StringMatch, service, GetServiceRE()))) {
    throw MakeError(kWrongServiceType, service);
  }

  // Provide defaults if matcher was not specified.
  if (IS_UNDEFINED(options)) {
    options = {};
  } else {
    options = TO_OBJECT(options);
  }

  var matcher = options.localeMatcher;
  if (!IS_UNDEFINED(matcher)) {
    matcher = GlobalString(matcher);
    if (matcher !== 'lookup' && matcher !== 'best fit') {
      throw MakeRangeError(kLocaleMatcher, matcher);
    }
  } else {
    matcher = 'best fit';
  }

  var requestedLocales = initializeLocaleList(locales);

  // Cache these, they don't ever change per service.
  if (IS_UNDEFINED(AVAILABLE_LOCALES[service])) {
    AVAILABLE_LOCALES[service] = getAvailableLocalesOf(service);
  }

  // Use either best fit or lookup algorithm to match locales.
  if (matcher === 'best fit') {
    return initializeLocaleList(bestFitSupportedLocalesOf(
        requestedLocales, AVAILABLE_LOCALES[service]));
  }

  return initializeLocaleList(lookupSupportedLocalesOf(
      requestedLocales, AVAILABLE_LOCALES[service]));
}


/**
 * Returns the subset of the provided BCP 47 language priority list for which
 * this service has a matching locale when using the BCP 47 Lookup algorithm.
 * Locales appear in the same order in the returned list as in the input list.
 */
function lookupSupportedLocalesOf(requestedLocales, availableLocales) {
  var matchedLocales = [];
  for (var i = 0; i < requestedLocales.length; ++i) {
    // Remove -u- extension.
    var locale = %_Call(StringReplace,
                        requestedLocales[i],
                        GetUnicodeExtensionRE(),
                        '');
    do {
      if (!IS_UNDEFINED(availableLocales[locale])) {
        // Push requested locale not the resolved one.
        %_Call(ArrayPush, matchedLocales, requestedLocales[i]);
        break;
      }
      // Truncate locale if possible, if not break.
      var pos = %_Call(StringLastIndexOf, locale, '-');
      if (pos === -1) {
        break;
      }
      locale = %_Call(StringSubstring, locale, 0, pos);
    } while (true);
  }

  return matchedLocales;
}


/**
 * Returns the subset of the provided BCP 47 language priority list for which
 * this service has a matching locale when using the implementation
 * dependent algorithm.
 * Locales appear in the same order in the returned list as in the input list.
 */
function bestFitSupportedLocalesOf(requestedLocales, availableLocales) {
  // TODO(cira): implement better best fit algorithm.
  return lookupSupportedLocalesOf(requestedLocales, availableLocales);
}


/**
 * Returns a getOption function that extracts property value for given
 * options object. If property is missing it returns defaultValue. If value
 * is out of range for that property it throws RangeError.
 */
function getGetOption(options, caller) {
  if (IS_UNDEFINED(options)) throw MakeError(kDefaultOptionsMissing, caller);

  var getOption = function getOption(property, type, values, defaultValue) {
    if (!IS_UNDEFINED(options[property])) {
      var value = options[property];
      switch (type) {
        case 'boolean':
          value = GlobalBoolean(value);
          break;
        case 'string':
          value = GlobalString(value);
          break;
        case 'number':
          value = GlobalNumber(value);
          break;
        default:
          throw MakeError(kWrongValueType);
      }

      if (!IS_UNDEFINED(values) && %_Call(ArrayIndexOf, values, value) === -1) {
        throw MakeRangeError(kValueOutOfRange, value, caller, property);
      }

      return value;
    }

    return defaultValue;
  }

  return getOption;
}


/**
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


/**
 * Returns best matched supported locale and extension info using basic
 * lookup algorithm.
 */
function lookupMatcher(service, requestedLocales) {
  if (IS_NULL(%_Call(StringMatch, service, GetServiceRE()))) {
    throw MakeError(kWrongServiceType, service);
  }

  // Cache these, they don't ever change per service.
  if (IS_UNDEFINED(AVAILABLE_LOCALES[service])) {
    AVAILABLE_LOCALES[service] = getAvailableLocalesOf(service);
  }

  for (var i = 0; i < requestedLocales.length; ++i) {
    // Remove all extensions.
    var locale = %_Call(StringReplace, requestedLocales[i],
                        GetAnyExtensionRE(), '');
    do {
      if (!IS_UNDEFINED(AVAILABLE_LOCALES[service][locale])) {
        // Return the resolved locale and extension.
        var extensionMatch =
            %_Call(StringMatch, requestedLocales[i], GetUnicodeExtensionRE());
        var extension = IS_NULL(extensionMatch) ? '' : extensionMatch[0];
        return {'locale': locale, 'extension': extension, 'position': i};
      }
      // Truncate locale if possible.
      var pos = %_Call(StringLastIndexOf, locale, '-');
      if (pos === -1) {
        break;
      }
      locale = %_Call(StringSubstring, locale, 0, pos);
    } while (true);
  }

  // Didn't find a match, return default.
  if (IS_UNDEFINED(DEFAULT_ICU_LOCALE)) {
    DEFAULT_ICU_LOCALE = %GetDefaultICULocale();
  }

  return {'locale': DEFAULT_ICU_LOCALE, 'extension': '', 'position': -1};
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
 * Parses Unicode extension into key - value map.
 * Returns empty object if the extension string is invalid.
 * We are not concerned with the validity of the values at this point.
 */
function parseExtension(extension) {
  var extensionSplit = %_Call(StringSplit, extension, '-');

  // Assume ['', 'u', ...] input, but don't throw.
  if (extensionSplit.length <= 2 ||
      (extensionSplit[0] !== '' && extensionSplit[1] !== 'u')) {
    return {};
  }

  // Key is {2}alphanum, value is {3,8}alphanum.
  // Some keys may not have explicit values (booleans).
  var extensionMap = {};
  var previousKey = UNDEFINED;
  for (var i = 2; i < extensionSplit.length; ++i) {
    var length = extensionSplit[i].length;
    var element = extensionSplit[i];
    if (length === 2) {
      extensionMap[element] = UNDEFINED;
      previousKey = element;
    } else if (length >= 3 && length <=8 && !IS_UNDEFINED(previousKey)) {
      extensionMap[previousKey] = element;
      previousKey = UNDEFINED;
    } else {
      // There is a value that's too long, or that doesn't have a key.
      return {};
    }
  }

  return extensionMap;
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
    return '-' + key + '-' + GlobalString(value);
  }

  var updateProperty = function updateProperty(property, type, value) {
    if (type === 'boolean' && (typeof value === 'string')) {
      value = (value === 'true') ? true : false;
    }

    if (!IS_UNDEFINED(property)) {
      defineWEProperty(outOptions, property, value);
    }
  }

  for (var key in keyValues) {
    if (%HasOwnProperty(keyValues, key)) {
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
      if (%HasOwnProperty(extensionMap, key)) {
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
 * Converts all OwnProperties into
 * configurable: false, writable: false, enumerable: true.
 */
function freezeArray(array) {
  var l = array.length;
  for (var i = 0; i < l; i++) {
    if (i in array) {
      ObjectDefineProperty(array, i, {value: array[i],
                                      configurable: false,
                                      writable: false,
                                      enumerable: true});
    }
  }

  ObjectDefineProperty(array, 'length', {value: l, writable: false});
  return array;
}


/**
 * It's sometimes desireable to leave user requested locale instead of ICU
 * supported one (zh-TW is equivalent to zh-Hant-TW, so we should keep shorter
 * one, if that was what user requested).
 * This function returns user specified tag if its maximized form matches ICU
 * resolved locale. If not we return ICU result.
 */
function getOptimalLanguageTag(original, resolved) {
  // Returns Array<Object>, where each object has maximized and base properties.
  // Maximized: zh -> zh-Hans-CN
  // Base: zh-CN-u-ca-gregory -> zh-CN
  // Take care of grandfathered or simple cases.
  if (original === resolved) {
    return original;
  }

  var locales = %GetLanguageTagVariants([original, resolved]);
  if (locales[0].maximized !== locales[1].maximized) {
    return resolved;
  }

  // Preserve extensions of resolved locale, but swap base tags with original.
  var resolvedBase = new GlobalRegExp('^' + locales[1].base);
  return %_Call(StringReplace, resolved, resolvedBase, locales[0].base);
}


/**
 * Returns an Object that contains all of supported locales for a given
 * service.
 * In addition to the supported locales we add xx-ZZ locale for each xx-Yyyy-ZZ
 * that is supported. This is required by the spec.
 */
function getAvailableLocalesOf(service) {
  var available = %AvailableLocalesOf(service);

  for (var i in available) {
    if (%HasOwnProperty(available, i)) {
      var parts =
          %_Call(StringMatch, i, /^([a-z]{2,3})-([A-Z][a-z]{3})-([A-Z]{2})$/);
      if (parts !== null) {
        // Build xx-ZZ. We don't care about the actual value,
        // as long it's not undefined.
        available[parts[1] + '-' + parts[3]] = null;
      }
    }
  }

  return available;
}


/**
 * Defines a property and sets writable and enumerable to true.
 * Configurable is false by default.
 */
function defineWEProperty(object, property, value) {
  ObjectDefineProperty(object, property,
                       {value: value, writable: true, enumerable: true});
}


/**
 * Adds property to an object if the value is not undefined.
 * Sets configurable descriptor to false.
 */
function addWEPropertyIfDefined(object, property, value) {
  if (!IS_UNDEFINED(value)) {
    defineWEProperty(object, property, value);
  }
}


/**
 * Defines a property and sets writable, enumerable and configurable to true.
 */
function defineWECProperty(object, property, value) {
  ObjectDefineProperty(object, property, {value: value,
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
  return %StringToUpperCase(%_Call(StringSubstr, word, 0, 1)) +
         %StringToLowerCase(%_Call(StringSubstr, word, 1));
}

/**
 * Returns titlecased location, bueNos_airES -> Buenos_Aires
 * or ho_cHi_minH -> Ho_Chi_Minh. It is locale-agnostic and only
 * deals with ASCII only characters.
 * 'of', 'au' and 'es' are special-cased and lowercased.
 */
function toTitleCaseTimezoneLocation(location) {
  var match = %_Call(StringMatch, location, GetTimezoneNameLocationPartRE());
  if (IS_NULL(match)) throw MakeRangeError(kExpectedLocation, location);

  var result = toTitleCaseWord(match[1]);
  if (!IS_UNDEFINED(match[2]) && 2 < match.length) {
    // The first character is a separator, '_' or '-'.
    // None of IANA zone names has both '_' and '-'.
    var separator = %_Call(StringSubstring, match[2], 0, 1);
    var parts = %_Call(StringSplit, match[2], separator);
    for (var i = 1; i < parts.length; i++) {
      var part = parts[i]
      var lowercasedPart = %StringToLowerCase(part);
      result = result + separator +
          ((lowercasedPart !== 'es' &&
            lowercasedPart !== 'of' && lowercasedPart !== 'au') ?
          toTitleCaseWord(part) : lowercasedPart);
    }
  }
  return result;
}

/**
 * Canonicalizes the language tag, or throws in case the tag is invalid.
 */
function canonicalizeLanguageTag(localeID) {
  // null is typeof 'object' so we have to do extra check.
  if (typeof localeID !== 'string' && typeof localeID !== 'object' ||
      IS_NULL(localeID)) {
    throw MakeTypeError(kLanguageID);
  }

  var localeString = GlobalString(localeID);

  if (isValidLanguageTag(localeString) === false) {
    throw MakeRangeError(kInvalidLanguageTag, localeString);
  }

  // This call will strip -kn but not -kn-true extensions.
  // ICU bug filled - http://bugs.icu-project.org/trac/ticket/9265.
  // TODO(cira): check if -u-kn-true-kc-true-kh-true still throws after
  // upgrade to ICU 4.9.
  var tag = %CanonicalizeLanguageTag(localeString);
  if (tag === 'invalid-tag') {
    throw MakeRangeError(kInvalidLanguageTag, localeString);
  }

  return tag;
}


/**
 * Returns an array where all locales are canonicalized and duplicates removed.
 * Throws on locales that are not well formed BCP47 tags.
 */
function initializeLocaleList(locales) {
  var seen = [];
  if (IS_UNDEFINED(locales)) {
    // Constructor is called without arguments.
    seen = [];
  } else {
    // We allow single string localeID.
    if (typeof locales === 'string') {
      %_Call(ArrayPush, seen, canonicalizeLanguageTag(locales));
      return freezeArray(seen);
    }

    var o = TO_OBJECT(locales);
    var len = TO_UINT32(o.length);

    for (var k = 0; k < len; k++) {
      if (k in o) {
        var value = o[k];

        var tag = canonicalizeLanguageTag(value);

        if (%_Call(ArrayIndexOf, seen, tag) === -1) {
          %_Call(ArrayPush, seen, tag);
        }
      }
    }
  }

  return freezeArray(seen);
}


/**
 * Validates the language tag. Section 2.2.9 of the bcp47 spec
 * defines a valid tag.
 *
 * ICU is too permissible and lets invalid tags, like
 * hant-cmn-cn, through.
 *
 * Returns false if the language tag is invalid.
 */
function isValidLanguageTag(locale) {
  // Check if it's well-formed, including grandfadered tags.
  if (!%_Call(RegExpTest, GetLanguageTagRE(), locale)) {
    return false;
  }

  // Just return if it's a x- form. It's all private.
  if (%_Call(StringIndexOf, locale, 'x-') === 0) {
    return true;
  }

  // Check if there are any duplicate variants or singletons (extensions).

  // Remove private use section.
  locale = %_Call(StringSplit, locale, /-x-/)[0];

  // Skip language since it can match variant regex, so we start from 1.
  // We are matching i-klingon here, but that's ok, since i-klingon-klingon
  // is not valid and would fail LANGUAGE_TAG_RE test.
  var variants = [];
  var extensions = [];
  var parts = %_Call(StringSplit, locale, /-/);
  for (var i = 1; i < parts.length; i++) {
    var value = parts[i];
    if (%_Call(RegExpTest, GetLanguageVariantRE(), value) &&
        extensions.length === 0) {
      if (%_Call(ArrayIndexOf, variants, value) === -1) {
        %_Call(ArrayPush, variants, value);
      } else {
        return false;
      }
    }

    if (%_Call(RegExpTest, GetLanguageSingletonRE(), value)) {
      if (%_Call(ArrayIndexOf, extensions, value) === -1) {
        %_Call(ArrayPush, extensions, value);
      } else {
        return false;
      }
    }
  }

  return true;
 }


/**
 * Builds a regular expresion that validates the language tag
 * against bcp47 spec.
 * Uses http://tools.ietf.org/html/bcp47, section 2.1, ABNF.
 * Runs on load and initializes the global REs.
 */
function BuildLanguageTagREs() {
  var alpha = '[a-zA-Z]';
  var digit = '[0-9]';
  var alphanum = '(' + alpha + '|' + digit + ')';
  var regular = '(art-lojban|cel-gaulish|no-bok|no-nyn|zh-guoyu|zh-hakka|' +
                'zh-min|zh-min-nan|zh-xiang)';
  var irregular = '(en-GB-oed|i-ami|i-bnn|i-default|i-enochian|i-hak|' +
                  'i-klingon|i-lux|i-mingo|i-navajo|i-pwn|i-tao|i-tay|' +
                  'i-tsu|sgn-BE-FR|sgn-BE-NL|sgn-CH-DE)';
  var grandfathered = '(' + irregular + '|' + regular + ')';
  var privateUse = '(x(-' + alphanum + '{1,8})+)';

  var singleton = '(' + digit + '|[A-WY-Za-wy-z])';
  LANGUAGE_SINGLETON_RE = new GlobalRegExp('^' + singleton + '$', 'i');

  var extension = '(' + singleton + '(-' + alphanum + '{2,8})+)';

  var variant = '(' + alphanum + '{5,8}|(' + digit + alphanum + '{3}))';
  LANGUAGE_VARIANT_RE = new GlobalRegExp('^' + variant + '$', 'i');

  var region = '(' + alpha + '{2}|' + digit + '{3})';
  var script = '(' + alpha + '{4})';
  var extLang = '(' + alpha + '{3}(-' + alpha + '{3}){0,2})';
  var language = '(' + alpha + '{2,3}(-' + extLang + ')?|' + alpha + '{4}|' +
                 alpha + '{5,8})';
  var langTag = language + '(-' + script + ')?(-' + region + ')?(-' +
                variant + ')*(-' + extension + ')*(-' + privateUse + ')?';

  var languageTag =
      '^(' + langTag + '|' + privateUse + '|' + grandfathered + ')$';
  LANGUAGE_TAG_RE = new GlobalRegExp(languageTag, 'i');
}

var resolvedAccessor = {
  get() {
    %IncrementUseCounter(kIntlResolved);
    return this[resolvedSymbol];
  },
  set(value) {
    this[resolvedSymbol] = value;
  }
};

/**
 * Initializes the given object so it's a valid Collator instance.
 * Useful for subclassing.
 */
function initializeCollator(collator, locales, options) {
  if (%IsInitializedIntlObject(collator)) {
    throw MakeTypeError(kReinitializeIntl, "Collator");
  }

  if (IS_UNDEFINED(options)) {
    options = {};
  }

  var getOption = getGetOption(options, 'collator');

  var internalOptions = {};

  defineWEProperty(internalOptions, 'usage', getOption(
    'usage', 'string', ['sort', 'search'], 'sort'));

  var sensitivity = getOption('sensitivity', 'string',
                              ['base', 'accent', 'case', 'variant']);
  if (IS_UNDEFINED(sensitivity) && internalOptions.usage === 'sort') {
    sensitivity = 'variant';
  }
  defineWEProperty(internalOptions, 'sensitivity', sensitivity);

  defineWEProperty(internalOptions, 'ignorePunctuation', getOption(
    'ignorePunctuation', 'boolean', UNDEFINED, false));

  var locale = resolveLocale('collator', locales, options);

  // ICU can't take kb, kc... parameters through localeID, so we need to pass
  // them as options.
  // One exception is -co- which has to be part of the extension, but only for
  // usage: sort, and its value can't be 'standard' or 'search'.
  var extensionMap = parseExtension(locale.extension);

  /**
   * Map of Unicode extensions to option properties, and their values and types,
   * for a collator.
   */
  var COLLATOR_KEY_MAP = {
    'kn': {'property': 'numeric', 'type': 'boolean'},
    'kf': {'property': 'caseFirst', 'type': 'string',
           'values': ['false', 'lower', 'upper']}
  };

  setOptions(
      options, extensionMap, COLLATOR_KEY_MAP, getOption, internalOptions);

  var collation = 'default';
  var extension = '';
  if (%HasOwnProperty(extensionMap, 'co') && internalOptions.usage === 'sort') {

    /**
     * Allowed -u-co- values. List taken from:
     * http://unicode.org/repos/cldr/trunk/common/bcp47/collation.xml
     */
    var ALLOWED_CO_VALUES = [
      'big5han', 'dict', 'direct', 'ducet', 'gb2312', 'phonebk', 'phonetic',
      'pinyin', 'reformed', 'searchjl', 'stroke', 'trad', 'unihan', 'zhuyin'
    ];

    if (%_Call(ArrayIndexOf, ALLOWED_CO_VALUES, extensionMap.co) !== -1) {
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
  // ObjectDefineProperties will either succeed defining or throw an error.
  var resolved = ObjectDefineProperties({}, {
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
  %MarkAsInitializedIntlObjectOfType(collator, 'collator', internalCollator);
  collator[resolvedSymbol] = resolved;
  ObjectDefineProperty(collator, 'resolved', resolvedAccessor);

  return collator;
}


/**
 * Constructs Intl.Collator object given optional locales and options
 * parameters.
 *
 * @constructor
 */
%AddNamedProperty(Intl, 'Collator', function() {
    var locales = %_Arguments(0);
    var options = %_Arguments(1);

    if (!this || this === Intl) {
      // Constructor is called as a function.
      return new Intl.Collator(locales, options);
    }

    return initializeCollator(TO_OBJECT(this), locales, options);
  },
  DONT_ENUM
);


/**
 * Collator resolvedOptions method.
 */
%AddNamedProperty(Intl.Collator.prototype, 'resolvedOptions', function() {
    if (!IS_UNDEFINED(new.target)) {
      throw MakeTypeError(kOrdinaryFunctionCalledAsConstructor);
    }

    if (!%IsInitializedIntlObjectOfType(this, 'collator')) {
      throw MakeTypeError(kResolvedOptionsCalledOnNonObject, "Collator");
    }

    var coll = this;
    var locale = getOptimalLanguageTag(coll[resolvedSymbol].requestedLocale,
                                       coll[resolvedSymbol].locale);

    return {
      locale: locale,
      usage: coll[resolvedSymbol].usage,
      sensitivity: coll[resolvedSymbol].sensitivity,
      ignorePunctuation: coll[resolvedSymbol].ignorePunctuation,
      numeric: coll[resolvedSymbol].numeric,
      caseFirst: coll[resolvedSymbol].caseFirst,
      collation: coll[resolvedSymbol].collation
    };
  },
  DONT_ENUM
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
%AddNamedProperty(Intl.Collator, 'supportedLocalesOf', function(locales) {
    if (!IS_UNDEFINED(new.target)) {
      throw MakeTypeError(kOrdinaryFunctionCalledAsConstructor);
    }

    return supportedLocalesOf('collator', locales, %_Arguments(1));
  },
  DONT_ENUM
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
  return %InternalCompare(%GetImplFromInitializedIntlObject(collator),
                          GlobalString(x), GlobalString(y));
};


addBoundMethod(Intl.Collator, 'compare', compare, 2);

/**
 * Verifies that the input is a well-formed ISO 4217 currency code.
 * Don't uppercase to test. It could convert invalid code into a valid one.
 * For example \u00DFP (Eszett+P) becomes SSP.
 */
function isWellFormedCurrencyCode(currency) {
  return typeof currency == "string" &&
      currency.length == 3 &&
      %_Call(StringMatch, currency, /[^A-Za-z]/) == null;
}


/**
 * Returns the valid digit count for a property, or throws RangeError on
 * a value out of the range.
 */
function getNumberOption(options, property, min, max, fallback) {
  var value = options[property];
  if (!IS_UNDEFINED(value)) {
    value = GlobalNumber(value);
    if (IsNaN(value) || value < min || value > max) {
      throw MakeRangeError(kPropertyValueOutOfRange, property);
    }
    return MathFloor(value);
  }

  return fallback;
}

var patternAccessor = {
  get() {
    %IncrementUseCounter(kIntlPattern);
    return this[patternSymbol];
  },
  set(value) {
    this[patternSymbol] = value;
  }
};

/**
 * Initializes the given object so it's a valid NumberFormat instance.
 * Useful for subclassing.
 */
function initializeNumberFormat(numberFormat, locales, options) {
  if (%IsInitializedIntlObject(numberFormat)) {
    throw MakeTypeError(kReinitializeIntl, "NumberFormat");
  }

  if (IS_UNDEFINED(options)) {
    options = {};
  }

  var getOption = getGetOption(options, 'numberformat');

  var locale = resolveLocale('numberformat', locales, options);

  var internalOptions = {};
  defineWEProperty(internalOptions, 'style', getOption(
    'style', 'string', ['decimal', 'percent', 'currency'], 'decimal'));

  var currency = getOption('currency', 'string');
  if (!IS_UNDEFINED(currency) && !isWellFormedCurrencyCode(currency)) {
    throw MakeRangeError(kInvalidCurrencyCode, currency);
  }

  if (internalOptions.style === 'currency' && IS_UNDEFINED(currency)) {
    throw MakeTypeError(kCurrencyCode);
  }

  var currencyDisplay = getOption(
      'currencyDisplay', 'string', ['code', 'symbol', 'name'], 'symbol');
  if (internalOptions.style === 'currency') {
    defineWEProperty(internalOptions, 'currency', %StringToUpperCase(currency));
    defineWEProperty(internalOptions, 'currencyDisplay', currencyDisplay);
  }

  // Digit ranges.
  var mnid = getNumberOption(options, 'minimumIntegerDigits', 1, 21, 1);
  defineWEProperty(internalOptions, 'minimumIntegerDigits', mnid);

  var mnfd = options['minimumFractionDigits'];
  var mxfd = options['maximumFractionDigits'];
  if (!IS_UNDEFINED(mnfd) || internalOptions.style !== 'currency') {
    mnfd = getNumberOption(options, 'minimumFractionDigits', 0, 20, 0);
    defineWEProperty(internalOptions, 'minimumFractionDigits', mnfd);
  }

  if (!IS_UNDEFINED(mxfd) || internalOptions.style !== 'currency') {
    var min_mxfd = internalOptions.style === 'percent' ? 0 : 3;
    mnfd = IS_UNDEFINED(mnfd) ? 0 : mnfd;
    var fallback_limit = (mnfd > min_mxfd) ? mnfd : min_mxfd;
    mxfd = getNumberOption(options, 'maximumFractionDigits', mnfd, 20, fallback_limit);
    defineWEProperty(internalOptions, 'maximumFractionDigits', mxfd);
  }

  var mnsd = options['minimumSignificantDigits'];
  var mxsd = options['maximumSignificantDigits'];
  if (!IS_UNDEFINED(mnsd) || !IS_UNDEFINED(mxsd)) {
    mnsd = getNumberOption(options, 'minimumSignificantDigits', 1, 21, 0);
    defineWEProperty(internalOptions, 'minimumSignificantDigits', mnsd);

    mxsd = getNumberOption(options, 'maximumSignificantDigits', mnsd, 21, 21);
    defineWEProperty(internalOptions, 'maximumSignificantDigits', mxsd);
  }

  // Grouping.
  defineWEProperty(internalOptions, 'useGrouping', getOption(
    'useGrouping', 'boolean', UNDEFINED, true));

  // ICU prefers options to be passed using -u- extension key/values for
  // number format, so we need to build that.
  var extensionMap = parseExtension(locale.extension);

  /**
   * Map of Unicode extensions to option properties, and their values and types,
   * for a number format.
   */
  var NUMBER_FORMAT_KEY_MAP = {
    'nu': {'property': UNDEFINED, 'type': 'string'}
  };

  var extension = setOptions(options, extensionMap, NUMBER_FORMAT_KEY_MAP,
                             getOption, internalOptions);

  var requestedLocale = locale.locale + extension;
  var resolved = ObjectDefineProperties({}, {
    currency: {writable: true},
    currencyDisplay: {writable: true},
    locale: {writable: true},
    maximumFractionDigits: {writable: true},
    minimumFractionDigits: {writable: true},
    minimumIntegerDigits: {writable: true},
    numberingSystem: {writable: true},
    pattern: patternAccessor,
    requestedLocale: {value: requestedLocale, writable: true},
    style: {value: internalOptions.style, writable: true},
    useGrouping: {writable: true}
  });
  if (%HasOwnProperty(internalOptions, 'minimumSignificantDigits')) {
    defineWEProperty(resolved, 'minimumSignificantDigits', UNDEFINED);
  }
  if (%HasOwnProperty(internalOptions, 'maximumSignificantDigits')) {
    defineWEProperty(resolved, 'maximumSignificantDigits', UNDEFINED);
  }
  var formatter = %CreateNumberFormat(requestedLocale,
                                      internalOptions,
                                      resolved);

  if (internalOptions.style === 'currency') {
    ObjectDefineProperty(resolved, 'currencyDisplay', {value: currencyDisplay,
                                                       writable: true});
  }

  %MarkAsInitializedIntlObjectOfType(numberFormat, 'numberformat', formatter);
  numberFormat[resolvedSymbol] = resolved;
  ObjectDefineProperty(numberFormat, 'resolved', resolvedAccessor);

  return numberFormat;
}


/**
 * Constructs Intl.NumberFormat object given optional locales and options
 * parameters.
 *
 * @constructor
 */
%AddNamedProperty(Intl, 'NumberFormat', function() {
    var locales = %_Arguments(0);
    var options = %_Arguments(1);

    if (!this || this === Intl) {
      // Constructor is called as a function.
      return new Intl.NumberFormat(locales, options);
    }

    return initializeNumberFormat(TO_OBJECT(this), locales, options);
  },
  DONT_ENUM
);


/**
 * NumberFormat resolvedOptions method.
 */
%AddNamedProperty(Intl.NumberFormat.prototype, 'resolvedOptions', function() {
    if (!IS_UNDEFINED(new.target)) {
      throw MakeTypeError(kOrdinaryFunctionCalledAsConstructor);
    }

    if (!%IsInitializedIntlObjectOfType(this, 'numberformat')) {
      throw MakeTypeError(kResolvedOptionsCalledOnNonObject, "NumberFormat");
    }

    var format = this;
    var locale = getOptimalLanguageTag(format[resolvedSymbol].requestedLocale,
                                       format[resolvedSymbol].locale);

    var result = {
      locale: locale,
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

    if (%HasOwnProperty(format[resolvedSymbol], 'minimumSignificantDigits')) {
      defineWECProperty(result, 'minimumSignificantDigits',
                        format[resolvedSymbol].minimumSignificantDigits);
    }

    if (%HasOwnProperty(format[resolvedSymbol], 'maximumSignificantDigits')) {
      defineWECProperty(result, 'maximumSignificantDigits',
                        format[resolvedSymbol].maximumSignificantDigits);
    }

    return result;
  },
  DONT_ENUM
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
%AddNamedProperty(Intl.NumberFormat, 'supportedLocalesOf', function(locales) {
    if (!IS_UNDEFINED(new.target)) {
      throw MakeTypeError(kOrdinaryFunctionCalledAsConstructor);
    }

    return supportedLocalesOf('numberformat', locales, %_Arguments(1));
  },
  DONT_ENUM
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
  // Spec treats -0 and +0 as 0.
  var number = TO_NUMBER(value) + 0;

  return %InternalNumberFormat(%GetImplFromInitializedIntlObject(formatter),
                               number);
}


/**
 * Returns a Number that represents string value that was passed in.
 */
function parseNumber(formatter, value) {
  return %InternalNumberParse(%GetImplFromInitializedIntlObject(formatter),
                              GlobalString(value));
}


addBoundMethod(Intl.NumberFormat, 'format', formatNumber, 1);
addBoundMethod(Intl.NumberFormat, 'v8Parse', parseNumber, 1);

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
  ldmlString = %_Call(StringReplace, ldmlString, GetQuotedStringRE(), '');

  var options = {};
  var match = %_Call(StringMatch, ldmlString, /E{3,5}/g);
  options = appendToDateTimeObject(
      options, 'weekday', match, {EEEEE: 'narrow', EEE: 'short', EEEE: 'long'});

  match = %_Call(StringMatch, ldmlString, /G{3,5}/g);
  options = appendToDateTimeObject(
      options, 'era', match, {GGGGG: 'narrow', GGG: 'short', GGGG: 'long'});

  match = %_Call(StringMatch, ldmlString, /y{1,2}/g);
  options = appendToDateTimeObject(
      options, 'year', match, {y: 'numeric', yy: '2-digit'});

  match = %_Call(StringMatch, ldmlString, /M{1,5}/g);
  options = appendToDateTimeObject(options, 'month', match, {MM: '2-digit',
      M: 'numeric', MMMMM: 'narrow', MMM: 'short', MMMM: 'long'});

  // Sometimes we get L instead of M for month - standalone name.
  match = %_Call(StringMatch, ldmlString, /L{1,5}/g);
  options = appendToDateTimeObject(options, 'month', match, {LL: '2-digit',
      L: 'numeric', LLLLL: 'narrow', LLL: 'short', LLLL: 'long'});

  match = %_Call(StringMatch, ldmlString, /d{1,2}/g);
  options = appendToDateTimeObject(
      options, 'day', match, {d: 'numeric', dd: '2-digit'});

  match = %_Call(StringMatch, ldmlString, /h{1,2}/g);
  if (match !== null) {
    options['hour12'] = true;
  }
  options = appendToDateTimeObject(
      options, 'hour', match, {h: 'numeric', hh: '2-digit'});

  match = %_Call(StringMatch, ldmlString, /H{1,2}/g);
  if (match !== null) {
    options['hour12'] = false;
  }
  options = appendToDateTimeObject(
      options, 'hour', match, {H: 'numeric', HH: '2-digit'});

  match = %_Call(StringMatch, ldmlString, /m{1,2}/g);
  options = appendToDateTimeObject(
      options, 'minute', match, {m: 'numeric', mm: '2-digit'});

  match = %_Call(StringMatch, ldmlString, /s{1,2}/g);
  options = appendToDateTimeObject(
      options, 'second', match, {s: 'numeric', ss: '2-digit'});

  match = %_Call(StringMatch, ldmlString, /z|zzzz/g);
  options = appendToDateTimeObject(
      options, 'timeZoneName', match, {z: 'short', zzzz: 'long'});

  return options;
}


function appendToDateTimeObject(options, option, match, pairs) {
  if (IS_NULL(match)) {
    if (!%HasOwnProperty(options, option)) {
      defineWEProperty(options, option, UNDEFINED);
    }
    return options;
  }

  var property = match[0];
  defineWEProperty(options, option, pairs[property]);

  return options;
}


/**
 * Returns options with at least default values in it.
 */
function toDateTimeOptions(options, required, defaults) {
  if (IS_UNDEFINED(options)) {
    options = {};
  } else {
    options = TO_OBJECT(options);
  }

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
    ObjectDefineProperty(options, 'year', {value: 'numeric',
                                           writable: true,
                                           enumerable: true,
                                           configurable: true});
    ObjectDefineProperty(options, 'month', {value: 'numeric',
                                            writable: true,
                                            enumerable: true,
                                            configurable: true});
    ObjectDefineProperty(options, 'day', {value: 'numeric',
                                          writable: true,
                                          enumerable: true,
                                          configurable: true});
  }

  if (needsDefault && (defaults === 'time' || defaults === 'all')) {
    ObjectDefineProperty(options, 'hour', {value: 'numeric',
                                           writable: true,
                                           enumerable: true,
                                           configurable: true});
    ObjectDefineProperty(options, 'minute', {value: 'numeric',
                                             writable: true,
                                             enumerable: true,
                                             configurable: true});
    ObjectDefineProperty(options, 'second', {value: 'numeric',
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
function initializeDateTimeFormat(dateFormat, locales, options) {

  if (%IsInitializedIntlObject(dateFormat)) {
    throw MakeTypeError(kReinitializeIntl, "DateTimeFormat");
  }

  if (IS_UNDEFINED(options)) {
    options = {};
  }

  var locale = resolveLocale('dateformat', locales, options);

  options = toDateTimeOptions(options, 'any', 'date');

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
  var internalOptions = {};
  var extensionMap = parseExtension(locale.extension);

  /**
   * Map of Unicode extensions to option properties, and their values and types,
   * for a date/time format.
   */
  var DATETIME_FORMAT_KEY_MAP = {
    'ca': {'property': UNDEFINED, 'type': 'string'},
    'nu': {'property': UNDEFINED, 'type': 'string'}
  };

  var extension = setOptions(options, extensionMap, DATETIME_FORMAT_KEY_MAP,
                             getOption, internalOptions);

  var requestedLocale = locale.locale + extension;
  var resolved = ObjectDefineProperties({}, {
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
    pattern: patternAccessor,
    requestedLocale: {value: requestedLocale, writable: true},
    second: {writable: true},
    timeZone: {writable: true},
    timeZoneName: {writable: true},
    tz: {value: tz, writable: true},
    weekday: {writable: true},
    year: {writable: true}
  });

  var formatter = %CreateDateTimeFormat(
    requestedLocale, {skeleton: ldmlString, timeZone: tz}, resolved);

  if (resolved.timeZone === "Etc/Unknown") {
    throw MakeRangeError(kUnsupportedTimeZone, tz);
  }

  %MarkAsInitializedIntlObjectOfType(dateFormat, 'dateformat', formatter);
  dateFormat[resolvedSymbol] = resolved;
  ObjectDefineProperty(dateFormat, 'resolved', resolvedAccessor);

  return dateFormat;
}


/**
 * Constructs Intl.DateTimeFormat object given optional locales and options
 * parameters.
 *
 * @constructor
 */
%AddNamedProperty(Intl, 'DateTimeFormat', function() {
    var locales = %_Arguments(0);
    var options = %_Arguments(1);

    if (!this || this === Intl) {
      // Constructor is called as a function.
      return new Intl.DateTimeFormat(locales, options);
    }

    return initializeDateTimeFormat(TO_OBJECT(this), locales, options);
  },
  DONT_ENUM
);


/**
 * DateTimeFormat resolvedOptions method.
 */
%AddNamedProperty(Intl.DateTimeFormat.prototype, 'resolvedOptions', function() {
    if (!IS_UNDEFINED(new.target)) {
      throw MakeTypeError(kOrdinaryFunctionCalledAsConstructor);
    }

    if (!%IsInitializedIntlObjectOfType(this, 'dateformat')) {
      throw MakeTypeError(kResolvedOptionsCalledOnNonObject, "DateTimeFormat");
    }

    /**
     * Maps ICU calendar names into LDML type.
     */
    var ICU_CALENDAR_MAP = {
      'gregorian': 'gregory',
      'japanese': 'japanese',
      'buddhist': 'buddhist',
      'roc': 'roc',
      'persian': 'persian',
      'islamic-civil': 'islamicc',
      'islamic': 'islamic',
      'hebrew': 'hebrew',
      'chinese': 'chinese',
      'indian': 'indian',
      'coptic': 'coptic',
      'ethiopic': 'ethiopic',
      'ethiopic-amete-alem': 'ethioaa'
    };

    var format = this;
    var fromPattern = fromLDMLString(format[resolvedSymbol][patternSymbol]);
    var userCalendar = ICU_CALENDAR_MAP[format[resolvedSymbol].calendar];
    if (IS_UNDEFINED(userCalendar)) {
      // Use ICU name if we don't have a match. It shouldn't happen, but
      // it would be too strict to throw for this.
      userCalendar = format[resolvedSymbol].calendar;
    }

    var locale = getOptimalLanguageTag(format[resolvedSymbol].requestedLocale,
                                       format[resolvedSymbol].locale);

    var result = {
      locale: locale,
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
  },
  DONT_ENUM
);
%FunctionSetName(Intl.DateTimeFormat.prototype.resolvedOptions,
                 'resolvedOptions');
%FunctionRemovePrototype(Intl.DateTimeFormat.prototype.resolvedOptions);
%SetNativeFlag(Intl.DateTimeFormat.prototype.resolvedOptions);


/**
 * Returns the subset of the given locale list for which this locale list
 * has a matching (possibly fallback) locale. Locales appear in the same
 * order in the returned list as in the input list.
 * Options are optional parameter.
 */
%AddNamedProperty(Intl.DateTimeFormat, 'supportedLocalesOf', function(locales) {
    if (!IS_UNDEFINED(new.target)) {
      throw MakeTypeError(kOrdinaryFunctionCalledAsConstructor);
    }

    return supportedLocalesOf('dateformat', locales, %_Arguments(1));
  },
  DONT_ENUM
);
%FunctionSetName(Intl.DateTimeFormat.supportedLocalesOf, 'supportedLocalesOf');
%FunctionRemovePrototype(Intl.DateTimeFormat.supportedLocalesOf);
%SetNativeFlag(Intl.DateTimeFormat.supportedLocalesOf);


/**
 * Returns a String value representing the result of calling ToNumber(date)
 * according to the effective locale and the formatting options of this
 * DateTimeFormat.
 */
function formatDate(formatter, dateValue) {
  var dateMs;
  if (IS_UNDEFINED(dateValue)) {
    dateMs = %DateCurrentTime();
  } else {
    dateMs = TO_NUMBER(dateValue);
  }

  if (!IsFinite(dateMs)) throw MakeRangeError(kDateRange);

  return %InternalDateFormat(%GetImplFromInitializedIntlObject(formatter),
                             new GlobalDate(dateMs));
}


/**
 * Returns a Date object representing the result of calling ToString(value)
 * according to the effective locale and the formatting options of this
 * DateTimeFormat.
 * Returns undefined if date string cannot be parsed.
 */
function parseDate(formatter, value) {
  return %InternalDateParse(%GetImplFromInitializedIntlObject(formatter),
                            GlobalString(value));
}


// 0 because date is optional argument.
addBoundMethod(Intl.DateTimeFormat, 'format', formatDate, 0);
addBoundMethod(Intl.DateTimeFormat, 'v8Parse', parseDate, 1);


/**
 * Returns canonical Area/Location(/Location) name, or throws an exception
 * if the zone name is invalid IANA name.
 */
function canonicalizeTimeZoneID(tzID) {
  // Skip undefined zones.
  if (IS_UNDEFINED(tzID)) {
    return tzID;
  }

  // Special case handling (UTC, GMT).
  var upperID = %StringToUpperCase(tzID);
  if (upperID === 'UTC' || upperID === 'GMT' ||
      upperID === 'ETC/UTC' || upperID === 'ETC/GMT') {
    return 'UTC';
  }

  // TODO(jshin): Add support for Etc/GMT[+-]([1-9]|1[0-2])

  // We expect only _, '-' and / beside ASCII letters.
  // All inputs should conform to Area/Location(/Location)* from now on.
  var match = %_Call(StringMatch, tzID, GetTimezoneNameCheckRE());
  if (IS_NULL(match)) throw MakeRangeError(kExpectedTimezoneID, tzID);

  var result = toTitleCaseTimezoneLocation(match[1]) + '/' +
               toTitleCaseTimezoneLocation(match[2]);

  if (!IS_UNDEFINED(match[3]) && 3 < match.length) {
    var locations = %_Call(StringSplit, match[3], '/');
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
function initializeBreakIterator(iterator, locales, options) {
  if (%IsInitializedIntlObject(iterator)) {
    throw MakeTypeError(kReinitializeIntl, "v8BreakIterator");
  }

  if (IS_UNDEFINED(options)) {
    options = {};
  }

  var getOption = getGetOption(options, 'breakiterator');

  var internalOptions = {};

  defineWEProperty(internalOptions, 'type', getOption(
    'type', 'string', ['character', 'word', 'sentence', 'line'], 'word'));

  var locale = resolveLocale('breakiterator', locales, options);
  var resolved = ObjectDefineProperties({}, {
    requestedLocale: {value: locale.locale, writable: true},
    type: {value: internalOptions.type, writable: true},
    locale: {writable: true}
  });

  var internalIterator = %CreateBreakIterator(locale.locale,
                                              internalOptions,
                                              resolved);

  %MarkAsInitializedIntlObjectOfType(iterator, 'breakiterator',
                                     internalIterator);
  iterator[resolvedSymbol] = resolved;
  ObjectDefineProperty(iterator, 'resolved', resolvedAccessor);

  return iterator;
}


/**
 * Constructs Intl.v8BreakIterator object given optional locales and options
 * parameters.
 *
 * @constructor
 */
%AddNamedProperty(Intl, 'v8BreakIterator', function() {
    var locales = %_Arguments(0);
    var options = %_Arguments(1);

    if (!this || this === Intl) {
      // Constructor is called as a function.
      return new Intl.v8BreakIterator(locales, options);
    }

    return initializeBreakIterator(TO_OBJECT(this), locales, options);
  },
  DONT_ENUM
);


/**
 * BreakIterator resolvedOptions method.
 */
%AddNamedProperty(Intl.v8BreakIterator.prototype, 'resolvedOptions',
  function() {
    if (!IS_UNDEFINED(new.target)) {
      throw MakeTypeError(kOrdinaryFunctionCalledAsConstructor);
    }

    if (!%IsInitializedIntlObjectOfType(this, 'breakiterator')) {
      throw MakeTypeError(kResolvedOptionsCalledOnNonObject, "v8BreakIterator");
    }

    var segmenter = this;
    var locale =
        getOptimalLanguageTag(segmenter[resolvedSymbol].requestedLocale,
                              segmenter[resolvedSymbol].locale);

    return {
      locale: locale,
      type: segmenter[resolvedSymbol].type
    };
  },
  DONT_ENUM
);
%FunctionSetName(Intl.v8BreakIterator.prototype.resolvedOptions,
                 'resolvedOptions');
%FunctionRemovePrototype(Intl.v8BreakIterator.prototype.resolvedOptions);
%SetNativeFlag(Intl.v8BreakIterator.prototype.resolvedOptions);


/**
 * Returns the subset of the given locale list for which this locale list
 * has a matching (possibly fallback) locale. Locales appear in the same
 * order in the returned list as in the input list.
 * Options are optional parameter.
 */
%AddNamedProperty(Intl.v8BreakIterator, 'supportedLocalesOf',
  function(locales) {
    if (!IS_UNDEFINED(new.target)) {
      throw MakeTypeError(kOrdinaryFunctionCalledAsConstructor);
    }

    return supportedLocalesOf('breakiterator', locales, %_Arguments(1));
  },
  DONT_ENUM
);
%FunctionSetName(Intl.v8BreakIterator.supportedLocalesOf, 'supportedLocalesOf');
%FunctionRemovePrototype(Intl.v8BreakIterator.supportedLocalesOf);
%SetNativeFlag(Intl.v8BreakIterator.supportedLocalesOf);


/**
 * Adopts text to segment using the iterator. Old text, if present,
 * gets discarded.
 */
function adoptText(iterator, text) {
  %BreakIteratorAdoptText(%GetImplFromInitializedIntlObject(iterator),
                          GlobalString(text));
}


/**
 * Returns index of the first break in the string and moves current pointer.
 */
function first(iterator) {
  return %BreakIteratorFirst(%GetImplFromInitializedIntlObject(iterator));
}


/**
 * Returns the index of the next break and moves the pointer.
 */
function next(iterator) {
  return %BreakIteratorNext(%GetImplFromInitializedIntlObject(iterator));
}


/**
 * Returns index of the current break.
 */
function current(iterator) {
  return %BreakIteratorCurrent(%GetImplFromInitializedIntlObject(iterator));
}


/**
 * Returns type of the current break.
 */
function breakType(iterator) {
  return %BreakIteratorBreakType(%GetImplFromInitializedIntlObject(iterator));
}


addBoundMethod(Intl.v8BreakIterator, 'adoptText', adoptText, 1);
addBoundMethod(Intl.v8BreakIterator, 'first', first, 0);
addBoundMethod(Intl.v8BreakIterator, 'next', next, 0);
addBoundMethod(Intl.v8BreakIterator, 'current', current, 0);
addBoundMethod(Intl.v8BreakIterator, 'breakType', breakType, 0);

// Save references to Intl objects and methods we use, for added security.
var savedObjects = {
  'collator': Intl.Collator,
  'numberformat': Intl.NumberFormat,
  'dateformatall': Intl.DateTimeFormat,
  'dateformatdate': Intl.DateTimeFormat,
  'dateformattime': Intl.DateTimeFormat
};


// Default (created with undefined locales and options parameters) collator,
// number and date format instances. They'll be created as needed.
var defaultObjects = {
  'collator': UNDEFINED,
  'numberformat': UNDEFINED,
  'dateformatall': UNDEFINED,
  'dateformatdate': UNDEFINED,
  'dateformattime': UNDEFINED,
};


/**
 * Returns cached or newly created instance of a given service.
 * We cache only default instances (where no locales or options are provided).
 */
function cachedOrNewService(service, locales, options, defaults) {
  var useOptions = (IS_UNDEFINED(defaults)) ? options : defaults;
  if (IS_UNDEFINED(locales) && IS_UNDEFINED(options)) {
    if (IS_UNDEFINED(defaultObjects[service])) {
      defaultObjects[service] = new savedObjects[service](locales, useOptions);
    }
    return defaultObjects[service];
  }
  return new savedObjects[service](locales, useOptions);
}


function OverrideFunction(object, name, f) {
  %CheckIsBootstrapping();
  ObjectDefineProperty(object, name, { value: f,
                                       writeable: true,
                                       configurable: true,
                                       enumerable: false });
  %FunctionSetName(f, name);
  %FunctionRemovePrototype(f);
  %SetNativeFlag(f);
}

/**
 * Compares this and that, and returns less than 0, 0 or greater than 0 value.
 * Overrides the built-in method.
 */
OverrideFunction(GlobalString.prototype, 'localeCompare', function(that) {
    if (!IS_UNDEFINED(new.target)) {
      throw MakeTypeError(kOrdinaryFunctionCalledAsConstructor);
    }

    if (IS_NULL_OR_UNDEFINED(this)) {
      throw MakeTypeError(kMethodInvokedOnNullOrUndefined);
    }

    var locales = %_Arguments(1);
    var options = %_Arguments(2);
    var collator = cachedOrNewService('collator', locales, options);
    return compare(collator, this, that);
  }
);


/**
 * Unicode normalization. This method is called with one argument that
 * specifies the normalization form.
 * If none is specified, "NFC" is assumed.
 * If the form is not one of "NFC", "NFD", "NFKC", or "NFKD", then throw
 * a RangeError Exception.
 */

OverrideFunction(GlobalString.prototype, 'normalize', function() {
    if (!IS_UNDEFINED(new.target)) {
      throw MakeTypeError(kOrdinaryFunctionCalledAsConstructor);
    }

    CHECK_OBJECT_COERCIBLE(this, "String.prototype.normalize");
    var s = TO_STRING(this);

    var formArg = %_Arguments(0);
    var form = IS_UNDEFINED(formArg) ? 'NFC' : TO_STRING(formArg);

    var NORMALIZATION_FORMS = ['NFC', 'NFD', 'NFKC', 'NFKD'];

    var normalizationForm = %_Call(ArrayIndexOf, NORMALIZATION_FORMS, form);
    if (normalizationForm === -1) {
      throw MakeRangeError(kNormalizationForm,
          %_Call(ArrayJoin, NORMALIZATION_FORMS, ', '));
    }

    return %StringNormalize(s, normalizationForm);
  }
);


/**
 * Formats a Number object (this) using locale and options values.
 * If locale or options are omitted, defaults are used.
 */
OverrideFunction(GlobalNumber.prototype, 'toLocaleString', function() {
    if (!IS_UNDEFINED(new.target)) {
      throw MakeTypeError(kOrdinaryFunctionCalledAsConstructor);
    }

    if (!(this instanceof GlobalNumber) && typeof(this) !== 'number') {
      throw MakeTypeError(kMethodInvokedOnWrongType, "Number");
    }

    var locales = %_Arguments(0);
    var options = %_Arguments(1);
    var numberFormat = cachedOrNewService('numberformat', locales, options);
    return formatNumber(numberFormat, this);
  }
);


/**
 * Returns actual formatted date or fails if date parameter is invalid.
 */
function toLocaleDateTime(date, locales, options, required, defaults, service) {
  if (!(date instanceof GlobalDate)) {
    throw MakeTypeError(kMethodInvokedOnWrongType, "Date");
  }

  if (IsNaN(date)) return 'Invalid Date';

  var internalOptions = toDateTimeOptions(options, required, defaults);

  var dateFormat =
      cachedOrNewService(service, locales, options, internalOptions);

  return formatDate(dateFormat, date);
}


/**
 * Formats a Date object (this) using locale and options values.
 * If locale or options are omitted, defaults are used - both date and time are
 * present in the output.
 */
OverrideFunction(GlobalDate.prototype, 'toLocaleString', function() {
    if (!IS_UNDEFINED(new.target)) {
      throw MakeTypeError(kOrdinaryFunctionCalledAsConstructor);
    }

    var locales = %_Arguments(0);
    var options = %_Arguments(1);
    return toLocaleDateTime(
        this, locales, options, 'any', 'all', 'dateformatall');
  }
);


/**
 * Formats a Date object (this) using locale and options values.
 * If locale or options are omitted, defaults are used - only date is present
 * in the output.
 */
OverrideFunction(GlobalDate.prototype, 'toLocaleDateString', function() {
    if (!IS_UNDEFINED(new.target)) {
      throw MakeTypeError(kOrdinaryFunctionCalledAsConstructor);
    }

    var locales = %_Arguments(0);
    var options = %_Arguments(1);
    return toLocaleDateTime(
        this, locales, options, 'date', 'date', 'dateformatdate');
  }
);


/**
 * Formats a Date object (this) using locale and options values.
 * If locale or options are omitted, defaults are used - only time is present
 * in the output.
 */
OverrideFunction(GlobalDate.prototype, 'toLocaleTimeString', function() {
    if (!IS_UNDEFINED(new.target)) {
      throw MakeTypeError(kOrdinaryFunctionCalledAsConstructor);
    }

    var locales = %_Arguments(0);
    var options = %_Arguments(1);
    return toLocaleDateTime(
        this, locales, options, 'time', 'time', 'dateformattime');
  }
);

})
