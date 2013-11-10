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

// ECMAScript 402 API implementation.

/**
 * Intl object is a single object that has some named properties,
 * all of which are constructors.
 */
$Object.defineProperty(global, "Intl", { enumerable: false, value: (function() {

'use strict';

var Intl = {};

var undefined = global.undefined;

var AVAILABLE_SERVICES = ['collator',
                          'numberformat',
                          'dateformat',
                          'breakiterator'];

/**
 * Caches available locales for each service.
 */
var AVAILABLE_LOCALES = {
  'collator': undefined,
  'numberformat': undefined,
  'dateformat': undefined,
  'breakiterator': undefined
};

/**
 * Caches default ICU locale.
 */
var DEFAULT_ICU_LOCALE = undefined;

/**
 * Unicode extension regular expression.
 */
var UNICODE_EXTENSION_RE = undefined;

function GetUnicodeExtensionRE() {
  if (UNICODE_EXTENSION_RE === undefined) {
    UNICODE_EXTENSION_RE = new $RegExp('-u(-[a-z0-9]{2,8})+', 'g');
  }
  return UNICODE_EXTENSION_RE;
}

/**
 * Matches any Unicode extension.
 */
var ANY_EXTENSION_RE = undefined;

function GetAnyExtensionRE() {
  if (ANY_EXTENSION_RE === undefined) {
    ANY_EXTENSION_RE = new $RegExp('-[a-z0-9]{1}-.*', 'g');
  }
  return ANY_EXTENSION_RE;
}

/**
 * Replace quoted text (single quote, anything but the quote and quote again).
 */
var QUOTED_STRING_RE = undefined;

function GetQuotedStringRE() {
  if (QUOTED_STRING_RE === undefined) {
    QUOTED_STRING_RE = new $RegExp("'[^']+'", 'g');
  }
  return QUOTED_STRING_RE;
}

/**
 * Matches valid service name.
 */
var SERVICE_RE = undefined;

function GetServiceRE() {
  if (SERVICE_RE === undefined) {
    SERVICE_RE =
        new $RegExp('^(collator|numberformat|dateformat|breakiterator)$');
  }
  return SERVICE_RE;
}

/**
 * Validates a language tag against bcp47 spec.
 * Actual value is assigned on first run.
 */
var LANGUAGE_TAG_RE = undefined;

function GetLanguageTagRE() {
  if (LANGUAGE_TAG_RE === undefined) {
    BuildLanguageTagREs();
  }
  return LANGUAGE_TAG_RE;
}

/**
 * Helps find duplicate variants in the language tag.
 */
var LANGUAGE_VARIANT_RE = undefined;

function GetLanguageVariantRE() {
  if (LANGUAGE_VARIANT_RE === undefined) {
    BuildLanguageTagREs();
  }
  return LANGUAGE_VARIANT_RE;
}

/**
 * Helps find duplicate singletons in the language tag.
 */
var LANGUAGE_SINGLETON_RE = undefined;

function GetLanguageSingletonRE() {
  if (LANGUAGE_SINGLETON_RE === undefined) {
    BuildLanguageTagREs();
  }
  return LANGUAGE_SINGLETON_RE;
}

/**
 * Matches valid IANA time zone names.
 */
var TIMEZONE_NAME_CHECK_RE = undefined;

function GetTimezoneNameCheckRE() {
  if (TIMEZONE_NAME_CHECK_RE === undefined) {
    TIMEZONE_NAME_CHECK_RE =
        new $RegExp('^([A-Za-z]+)/([A-Za-z]+)(?:_([A-Za-z]+))*$');
  }
  return TIMEZONE_NAME_CHECK_RE;
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

/**
 * Map of Unicode extensions to option properties, and their values and types,
 * for a collator.
 */
var COLLATOR_KEY_MAP = {
  'kn': {'property': 'numeric', 'type': 'boolean'},
  'kf': {'property': 'caseFirst', 'type': 'string',
         'values': ['false', 'lower', 'upper']}
};

/**
 * Map of Unicode extensions to option properties, and their values and types,
 * for a number format.
 */
var NUMBER_FORMAT_KEY_MAP = {
  'nu': {'property': undefined, 'type': 'string'}
};

/**
 * Map of Unicode extensions to option properties, and their values and types,
 * for a date/time format.
 */
var DATETIME_FORMAT_KEY_MAP = {
  'ca': {'property': undefined, 'type': 'string'},
  'nu': {'property': undefined, 'type': 'string'}
};

/**
 * Allowed -u-co- values. List taken from:
 * http://unicode.org/repos/cldr/trunk/common/bcp47/collation.xml
 */
var ALLOWED_CO_VALUES = [
  'big5han', 'dict', 'direct', 'ducet', 'gb2312', 'phonebk', 'phonetic',
  'pinyin', 'reformed', 'searchjl', 'stroke', 'trad', 'unihan', 'zhuyin'
];

/**
 * Error message for when function object is created with new and it's not
 * a constructor.
 */
var ORDINARY_FUNCTION_CALLED_AS_CONSTRUCTOR =
  'Function object that\'s not a constructor was created with new';


/**
 * Adds bound method to the prototype of the given object.
 */
function addBoundMethod(obj, methodName, implementation, length) {
  function getter() {
    if (!this || typeof this !== 'object' ||
        this.__initializedIntlObject === undefined) {
        throw new $TypeError('Method ' + methodName + ' called on a ' +
                            'non-object or on a wrong type of object.');
    }
    var internalName = '__bound' + methodName + '__';
    if (this[internalName] === undefined) {
      var that = this;
      var boundMethod;
      if (length === undefined || length === 2) {
        boundMethod = function(x, y) {
          if (%_IsConstructCall()) {
            throw new $TypeError(ORDINARY_FUNCTION_CALLED_AS_CONSTRUCTOR);
          }
          return implementation(that, x, y);
        }
      } else if (length === 1) {
        boundMethod = function(x) {
          if (%_IsConstructCall()) {
            throw new $TypeError(ORDINARY_FUNCTION_CALLED_AS_CONSTRUCTOR);
          }
          return implementation(that, x);
        }
      } else {
        boundMethod = function() {
          if (%_IsConstructCall()) {
            throw new $TypeError(ORDINARY_FUNCTION_CALLED_AS_CONSTRUCTOR);
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

  $Object.defineProperty(obj.prototype, methodName, {
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
  if (IS_NULL(service.match(GetServiceRE()))) {
    throw new $Error('Internal error, wrong service type: ' + service);
  }

  // Provide defaults if matcher was not specified.
  if (options === undefined) {
    options = {};
  } else {
    options = toObject(options);
  }

  var matcher = options.localeMatcher;
  if (matcher !== undefined) {
    matcher = $String(matcher);
    if (matcher !== 'lookup' && matcher !== 'best fit') {
      throw new $RangeError('Illegal value for localeMatcher:' + matcher);
    }
  } else {
    matcher = 'best fit';
  }

  var requestedLocales = initializeLocaleList(locales);

  // Cache these, they don't ever change per service.
  if (AVAILABLE_LOCALES[service] === undefined) {
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
    var locale = requestedLocales[i].replace(GetUnicodeExtensionRE(), '');
    do {
      if (availableLocales[locale] !== undefined) {
        // Push requested locale not the resolved one.
        matchedLocales.push(requestedLocales[i]);
        break;
      }
      // Truncate locale if possible, if not break.
      var pos = locale.lastIndexOf('-');
      if (pos === -1) {
        break;
      }
      locale = locale.substring(0, pos);
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
  if (options === undefined) {
    throw new $Error('Internal ' + caller + ' error. ' +
                    'Default options are missing.');
  }

  var getOption = function getOption(property, type, values, defaultValue) {
    if (options[property] !== undefined) {
      var value = options[property];
      switch (type) {
        case 'boolean':
          value = $Boolean(value);
          break;
        case 'string':
          value = $String(value);
          break;
        case 'number':
          value = $Number(value);
          break;
        default:
          throw new $Error('Internal error. Wrong value type.');
      }
      if (values !== undefined && values.indexOf(value) === -1) {
        throw new $RangeError('Value ' + value + ' out of range for ' + caller +
                             ' options property ' + property);
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
  if (IS_NULL(service.match(GetServiceRE()))) {
    throw new $Error('Internal error, wrong service type: ' + service);
  }

  // Cache these, they don't ever change per service.
  if (AVAILABLE_LOCALES[service] === undefined) {
    AVAILABLE_LOCALES[service] = getAvailableLocalesOf(service);
  }

  for (var i = 0; i < requestedLocales.length; ++i) {
    // Remove all extensions.
    var locale = requestedLocales[i].replace(GetAnyExtensionRE(), '');
    do {
      if (AVAILABLE_LOCALES[service][locale] !== undefined) {
        // Return the resolved locale and extension.
        var extensionMatch = requestedLocales[i].match(GetUnicodeExtensionRE());
        var extension = IS_NULL(extensionMatch) ? '' : extensionMatch[0];
        return {'locale': locale, 'extension': extension, 'position': i};
      }
      // Truncate locale if possible.
      var pos = locale.lastIndexOf('-');
      if (pos === -1) {
        break;
      }
      locale = locale.substring(0, pos);
    } while (true);
  }

  // Didn't find a match, return default.
  if (DEFAULT_ICU_LOCALE === undefined) {
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
  var extensionSplit = extension.split('-');

  // Assume ['', 'u', ...] input, but don't throw.
  if (extensionSplit.length <= 2 ||
      (extensionSplit[0] !== '' && extensionSplit[1] !== 'u')) {
    return {};
  }

  // Key is {2}alphanum, value is {3,8}alphanum.
  // Some keys may not have explicit values (booleans).
  var extensionMap = {};
  var previousKey = undefined;
  for (var i = 2; i < extensionSplit.length; ++i) {
    var length = extensionSplit[i].length;
    var element = extensionSplit[i];
    if (length === 2) {
      extensionMap[element] = undefined;
      previousKey = element;
    } else if (length >= 3 && length <=8 && previousKey !== undefined) {
      extensionMap[previousKey] = element;
      previousKey = undefined;
    } else {
      // There is a value that's too long, or that doesn't have a key.
      return {};
    }
  }

  return extensionMap;
}


/**
 * Converts parameter to an Object if possible.
 */
function toObject(value) {
  if (IS_NULL_OR_UNDEFINED(value)) {
    throw new $TypeError('Value cannot be converted to an Object.');
  }

  return $Object(value);
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
    return '-' + key + '-' + $String(value);
  }

  var updateProperty = function updateProperty(property, type, value) {
    if (type === 'boolean' && (typeof value === 'string')) {
      value = (value === 'true') ? true : false;
    }

    if (property !== undefined) {
      defineWEProperty(outOptions, property, value);
    }
  }

  for (var key in keyValues) {
    if (keyValues.hasOwnProperty(key)) {
      var value = undefined;
      var map = keyValues[key];
      if (map.property !== undefined) {
        // This may return true if user specifies numeric: 'false', since
        // Boolean('nonempty') === true.
        value = getOption(map.property, map.type, map.values);
      }
      if (value !== undefined) {
        updateProperty(map.property, map.type, value);
        extension += updateExtension(key, value);
        continue;
      }
      // User options didn't have it, check Unicode extension.
      // Here we want to convert strings 'true', 'false' into proper Boolean
      // values (not a user error).
      if (extensionMap.hasOwnProperty(key)) {
        value = extensionMap[key];
        if (value !== undefined) {
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
  array.forEach(function(element, index) {
    $Object.defineProperty(array, index, {value: element,
                                          configurable: false,
                                          writable: false,
                                          enumerable: true});
  });

  $Object.defineProperty(array, 'length', {value: array.length,
                                           writable: false});

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
  var resolvedBase = new $RegExp('^' + locales[1].base);
  return resolved.replace(resolvedBase, locales[0].base);
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
    if (available.hasOwnProperty(i)) {
      var parts = i.match(/^([a-z]{2,3})-([A-Z][a-z]{3})-([A-Z]{2})$/);
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
  $Object.defineProperty(object, property,
                         {value: value, writable: true, enumerable: true});
}


/**
 * Adds property to an object if the value is not undefined.
 * Sets configurable descriptor to false.
 */
function addWEPropertyIfDefined(object, property, value) {
  if (value !== undefined) {
    defineWEProperty(object, property, value);
  }
}


/**
 * Defines a property and sets writable, enumerable and configurable to true.
 */
function defineWECProperty(object, property, value) {
  $Object.defineProperty(object, property,
                         {value: value,
                          writable: true,
                          enumerable: true,
                          configurable: true});
}


/**
 * Adds property to an object if the value is not undefined.
 * Sets all descriptors to true.
 */
function addWECPropertyIfDefined(object, property, value) {
  if (value !== undefined) {
    defineWECProperty(object, property, value);
  }
}


/**
 * Returns titlecased word, aMeRricA -> America.
 */
function toTitleCaseWord(word) {
  return word.substr(0, 1).toUpperCase() + word.substr(1).toLowerCase();
}

/**
 * Canonicalizes the language tag, or throws in case the tag is invalid.
 */
function canonicalizeLanguageTag(localeID) {
  // null is typeof 'object' so we have to do extra check.
  if (typeof localeID !== 'string' && typeof localeID !== 'object' ||
      IS_NULL(localeID)) {
    throw new $TypeError('Language ID should be string or object.');
  }

  var localeString = $String(localeID);

  if (isValidLanguageTag(localeString) === false) {
    throw new $RangeError('Invalid language tag: ' + localeString);
  }

  // This call will strip -kn but not -kn-true extensions.
  // ICU bug filled - http://bugs.icu-project.org/trac/ticket/9265.
  // TODO(cira): check if -u-kn-true-kc-true-kh-true still throws after
  // upgrade to ICU 4.9.
  var tag = %CanonicalizeLanguageTag(localeString);
  if (tag === 'invalid-tag') {
    throw new $RangeError('Invalid language tag: ' + localeString);
  }

  return tag;
}


/**
 * Returns an array where all locales are canonicalized and duplicates removed.
 * Throws on locales that are not well formed BCP47 tags.
 */
function initializeLocaleList(locales) {
  var seen = [];
  if (locales === undefined) {
    // Constructor is called without arguments.
    seen = [];
  } else {
    // We allow single string localeID.
    if (typeof locales === 'string') {
      seen.push(canonicalizeLanguageTag(locales));
      return freezeArray(seen);
    }

    var o = toObject(locales);
    // Converts it to UInt32 (>>> is shr on 32bit integers).
    var len = o.length >>> 0;

    for (var k = 0; k < len; k++) {
      if (k in o) {
        var value = o[k];

        var tag = canonicalizeLanguageTag(value);

        if (seen.indexOf(tag) === -1) {
          seen.push(tag);
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
  if (GetLanguageTagRE().test(locale) === false) {
    return false;
  }

  // Just return if it's a x- form. It's all private.
  if (locale.indexOf('x-') === 0) {
    return true;
  }

  // Check if there are any duplicate variants or singletons (extensions).

  // Remove private use section.
  locale = locale.split(/-x-/)[0];

  // Skip language since it can match variant regex, so we start from 1.
  // We are matching i-klingon here, but that's ok, since i-klingon-klingon
  // is not valid and would fail LANGUAGE_TAG_RE test.
  var variants = [];
  var extensions = [];
  var parts = locale.split(/-/);
  for (var i = 1; i < parts.length; i++) {
    var value = parts[i];
    if (GetLanguageVariantRE().test(value) === true && extensions.length === 0) {
      if (variants.indexOf(value) === -1) {
        variants.push(value);
      } else {
        return false;
      }
    }

    if (GetLanguageSingletonRE().test(value) === true) {
      if (extensions.indexOf(value) === -1) {
        extensions.push(value);
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
  LANGUAGE_SINGLETON_RE = new $RegExp('^' + singleton + '$', 'i');

  var extension = '(' + singleton + '(-' + alphanum + '{2,8})+)';

  var variant = '(' + alphanum + '{5,8}|(' + digit + alphanum + '{3}))';
  LANGUAGE_VARIANT_RE = new $RegExp('^' + variant + '$', 'i');

  var region = '(' + alpha + '{2}|' + digit + '{3})';
  var script = '(' + alpha + '{4})';
  var extLang = '(' + alpha + '{3}(-' + alpha + '{3}){0,2})';
  var language = '(' + alpha + '{2,3}(-' + extLang + ')?|' + alpha + '{4}|' +
                 alpha + '{5,8})';
  var langTag = language + '(-' + script + ')?(-' + region + ')?(-' +
                variant + ')*(-' + extension + ')*(-' + privateUse + ')?';

  var languageTag =
      '^(' + langTag + '|' + privateUse + '|' + grandfathered + ')$';
  LANGUAGE_TAG_RE = new $RegExp(languageTag, 'i');
}

/**
 * Initializes the given object so it's a valid Collator instance.
 * Useful for subclassing.
 */
function initializeCollator(collator, locales, options) {
  if (collator.hasOwnProperty('__initializedIntlObject')) {
    throw new $TypeError('Trying to re-initialize Collator object.');
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
  var resolved = $Object.defineProperties({}, {
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
  $Object.defineProperty(collator, 'collator', {value: internalCollator});
  $Object.defineProperty(collator, '__initializedIntlObject',
                         {value: 'collator'});
  $Object.defineProperty(collator, 'resolved', {value: resolved});

  return collator;
}


/**
 * Constructs Intl.Collator object given optional locales and options
 * parameters.
 *
 * @constructor
 */
%SetProperty(Intl, 'Collator', function() {
    var locales = %_Arguments(0);
    var options = %_Arguments(1);

    if (!this || this === Intl) {
      // Constructor is called as a function.
      return new Intl.Collator(locales, options);
    }

    return initializeCollator(toObject(this), locales, options);
  },
  DONT_ENUM
);


/**
 * Collator resolvedOptions method.
 */
%SetProperty(Intl.Collator.prototype, 'resolvedOptions', function() {
    if (%_IsConstructCall()) {
      throw new $TypeError(ORDINARY_FUNCTION_CALLED_AS_CONSTRUCTOR);
    }

    if (!this || typeof this !== 'object' ||
        this.__initializedIntlObject !== 'collator') {
      throw new $TypeError('resolvedOptions method called on a non-object ' +
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
%SetProperty(Intl.Collator, 'supportedLocalesOf', function(locales) {
    if (%_IsConstructCall()) {
      throw new $TypeError(ORDINARY_FUNCTION_CALLED_AS_CONSTRUCTOR);
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
  return %InternalCompare(collator.collator, $String(x), $String(y));
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
      currency.match(/[^A-Za-z]/) == null;
}


/**
 * Returns the valid digit count for a property, or throws RangeError on
 * a value out of the range.
 */
function getNumberOption(options, property, min, max, fallback) {
  var value = options[property];
  if (value !== undefined) {
    value = $Number(value);
    if ($isNaN(value) || value < min || value > max) {
      throw new $RangeError(property + ' value is out of range.');
    }
    return $floor(value);
  }

  return fallback;
}


/**
 * Initializes the given object so it's a valid NumberFormat instance.
 * Useful for subclassing.
 */
function initializeNumberFormat(numberFormat, locales, options) {
  if (numberFormat.hasOwnProperty('__initializedIntlObject')) {
    throw new $TypeError('Trying to re-initialize NumberFormat object.');
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
    throw new $RangeError('Invalid currency code: ' + currency);
  }

  if (internalOptions.style === 'currency' && currency === undefined) {
    throw new $TypeError('Currency code is required with currency style.');
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
  var resolved = $Object.defineProperties({}, {
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
  var formatter = %CreateNumberFormat(requestedLocale,
                                      internalOptions,
                                      resolved);

  // We can't get information about number or currency style from ICU, so we
  // assume user request was fulfilled.
  if (internalOptions.style === 'currency') {
    $Object.defineProperty(resolved, 'currencyDisplay', {value: currencyDisplay,
                                                         writable: true});
  }

  $Object.defineProperty(numberFormat, 'formatter', {value: formatter});
  $Object.defineProperty(numberFormat, 'resolved', {value: resolved});
  $Object.defineProperty(numberFormat, '__initializedIntlObject',
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
    var locales = %_Arguments(0);
    var options = %_Arguments(1);

    if (!this || this === Intl) {
      // Constructor is called as a function.
      return new Intl.NumberFormat(locales, options);
    }

    return initializeNumberFormat(toObject(this), locales, options);
  },
  DONT_ENUM
);


/**
 * NumberFormat resolvedOptions method.
 */
%SetProperty(Intl.NumberFormat.prototype, 'resolvedOptions', function() {
    if (%_IsConstructCall()) {
      throw new $TypeError(ORDINARY_FUNCTION_CALLED_AS_CONSTRUCTOR);
    }

    if (!this || typeof this !== 'object' ||
        this.__initializedIntlObject !== 'numberformat') {
      throw new $TypeError('resolvedOptions method called on a non-object' +
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
%SetProperty(Intl.NumberFormat, 'supportedLocalesOf', function(locales) {
    if (%_IsConstructCall()) {
      throw new $TypeError(ORDINARY_FUNCTION_CALLED_AS_CONSTRUCTOR);
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
  var number = $Number(value);
  if (number === -0) {
    number = 0;
  }

  return %InternalNumberFormat(formatter.formatter, number);
}


/**
 * Returns a Number that represents string value that was passed in.
 */
function parseNumber(formatter, value) {
  return %InternalNumberParse(formatter.formatter, $String(value));
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
  if (hr12 === undefined) {
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
  if (option !== undefined) {
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
  ldmlString = ldmlString.replace(GetQuotedStringRE(), '');

  var options = {};
  var match = ldmlString.match(/E{3,5}/g);
  options = appendToDateTimeObject(
      options, 'weekday', match, {EEEEE: 'narrow', EEE: 'short', EEEE: 'long'});

  match = ldmlString.match(/G{3,5}/g);
  options = appendToDateTimeObject(
      options, 'era', match, {GGGGG: 'narrow', GGG: 'short', GGGG: 'long'});

  match = ldmlString.match(/y{1,2}/g);
  options = appendToDateTimeObject(
      options, 'year', match, {y: 'numeric', yy: '2-digit'});

  match = ldmlString.match(/M{1,5}/g);
  options = appendToDateTimeObject(options, 'month', match, {MM: '2-digit',
      M: 'numeric', MMMMM: 'narrow', MMM: 'short', MMMM: 'long'});

  // Sometimes we get L instead of M for month - standalone name.
  match = ldmlString.match(/L{1,5}/g);
  options = appendToDateTimeObject(options, 'month', match, {LL: '2-digit',
      L: 'numeric', LLLLL: 'narrow', LLL: 'short', LLLL: 'long'});

  match = ldmlString.match(/d{1,2}/g);
  options = appendToDateTimeObject(
      options, 'day', match, {d: 'numeric', dd: '2-digit'});

  match = ldmlString.match(/h{1,2}/g);
  if (match !== null) {
    options['hour12'] = true;
  }
  options = appendToDateTimeObject(
      options, 'hour', match, {h: 'numeric', hh: '2-digit'});

  match = ldmlString.match(/H{1,2}/g);
  if (match !== null) {
    options['hour12'] = false;
  }
  options = appendToDateTimeObject(
      options, 'hour', match, {H: 'numeric', HH: '2-digit'});

  match = ldmlString.match(/m{1,2}/g);
  options = appendToDateTimeObject(
      options, 'minute', match, {m: 'numeric', mm: '2-digit'});

  match = ldmlString.match(/s{1,2}/g);
  options = appendToDateTimeObject(
      options, 'second', match, {s: 'numeric', ss: '2-digit'});

  match = ldmlString.match(/z|zzzz/g);
  options = appendToDateTimeObject(
      options, 'timeZoneName', match, {z: 'short', zzzz: 'long'});

  return options;
}


function appendToDateTimeObject(options, option, match, pairs) {
  if (IS_NULL(match)) {
    if (!options.hasOwnProperty(option)) {
      defineWEProperty(options, option, undefined);
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
  if (options === undefined) {
    options = null;
  } else {
    options = toObject(options);
  }

  options = $Object.apply(this, [options]);

  var needsDefault = true;
  if ((required === 'date' || required === 'any') &&
      (options.weekday !== undefined || options.year !== undefined ||
       options.month !== undefined || options.day !== undefined)) {
    needsDefault = false;
  }

  if ((required === 'time' || required === 'any') &&
      (options.hour !== undefined || options.minute !== undefined ||
       options.second !== undefined)) {
    needsDefault = false;
  }

  if (needsDefault && (defaults === 'date' || defaults === 'all')) {
    $Object.defineProperty(options, 'year', {value: 'numeric',
                                             writable: true,
                                             enumerable: true,
                                             configurable: true});
    $Object.defineProperty(options, 'month', {value: 'numeric',
                                              writable: true,
                                              enumerable: true,
                                              configurable: true});
    $Object.defineProperty(options, 'day', {value: 'numeric',
                                            writable: true,
                                            enumerable: true,
                                            configurable: true});
  }

  if (needsDefault && (defaults === 'time' || defaults === 'all')) {
    $Object.defineProperty(options, 'hour', {value: 'numeric',
                                             writable: true,
                                             enumerable: true,
                                             configurable: true});
    $Object.defineProperty(options, 'minute', {value: 'numeric',
                                               writable: true,
                                               enumerable: true,
                                               configurable: true});
    $Object.defineProperty(options, 'second', {value: 'numeric',
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

  if (dateFormat.hasOwnProperty('__initializedIntlObject')) {
    throw new $TypeError('Trying to re-initialize DateTimeFormat object.');
  }

  if (options === undefined) {
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
  var extension = setOptions(options, extensionMap, DATETIME_FORMAT_KEY_MAP,
                             getOption, internalOptions);

  var requestedLocale = locale.locale + extension;
  var resolved = $Object.defineProperties({}, {
    calendar: {writable: true},
    day: {writable: true},
    era: {writable: true},
    hour12: {writable: true},
    hour: {writable: true},
    locale: {writable: true},
    minute: {writable: true},
    month: {writable: true},
    numberingSystem: {writable: true},
    pattern: {writable: true},
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

  if (tz !== undefined && tz !== resolved.timeZone) {
    throw new $RangeError('Unsupported time zone specified ' + tz);
  }

  $Object.defineProperty(dateFormat, 'formatter', {value: formatter});
  $Object.defineProperty(dateFormat, 'resolved', {value: resolved});
  $Object.defineProperty(dateFormat, '__initializedIntlObject',
                         {value: 'dateformat'});

  return dateFormat;
}


/**
 * Constructs Intl.DateTimeFormat object given optional locales and options
 * parameters.
 *
 * @constructor
 */
%SetProperty(Intl, 'DateTimeFormat', function() {
    var locales = %_Arguments(0);
    var options = %_Arguments(1);

    if (!this || this === Intl) {
      // Constructor is called as a function.
      return new Intl.DateTimeFormat(locales, options);
    }

    return initializeDateTimeFormat(toObject(this), locales, options);
  },
  DONT_ENUM
);


/**
 * DateTimeFormat resolvedOptions method.
 */
%SetProperty(Intl.DateTimeFormat.prototype, 'resolvedOptions', function() {
    if (%_IsConstructCall()) {
      throw new $TypeError(ORDINARY_FUNCTION_CALLED_AS_CONSTRUCTOR);
    }

    if (!this || typeof this !== 'object' ||
        this.__initializedIntlObject !== 'dateformat') {
      throw new $TypeError('resolvedOptions method called on a non-object or ' +
          'on a object that is not Intl.DateTimeFormat.');
    }

    var format = this;
    var fromPattern = fromLDMLString(format.resolved.pattern);
    var userCalendar = ICU_CALENDAR_MAP[format.resolved.calendar];
    if (userCalendar === undefined) {
      // Use ICU name if we don't have a match. It shouldn't happen, but
      // it would be too strict to throw for this.
      userCalendar = format.resolved.calendar;
    }

    var locale = getOptimalLanguageTag(format.resolved.requestedLocale,
                                       format.resolved.locale);

    var result = {
      locale: locale,
      numberingSystem: format.resolved.numberingSystem,
      calendar: userCalendar,
      timeZone: format.resolved.timeZone
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
%SetProperty(Intl.DateTimeFormat, 'supportedLocalesOf', function(locales) {
    if (%_IsConstructCall()) {
      throw new $TypeError(ORDINARY_FUNCTION_CALLED_AS_CONSTRUCTOR);
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
  if (dateValue === undefined) {
    dateMs = $Date.now();
  } else {
    dateMs = $Number(dateValue);
  }

  if (!$isFinite(dateMs)) {
    throw new $RangeError('Provided date is not in valid range.');
  }

  return %InternalDateFormat(formatter.formatter, new $Date(dateMs));
}


/**
 * Returns a Date object representing the result of calling ToString(value)
 * according to the effective locale and the formatting options of this
 * DateTimeFormat.
 * Returns undefined if date string cannot be parsed.
 */
function parseDate(formatter, value) {
  return %InternalDateParse(formatter.formatter, $String(value));
}


// 0 because date is optional argument.
addBoundMethod(Intl.DateTimeFormat, 'format', formatDate, 0);
addBoundMethod(Intl.DateTimeFormat, 'v8Parse', parseDate, 1);


/**
 * Returns canonical Area/Location name, or throws an exception if the zone
 * name is invalid IANA name.
 */
function canonicalizeTimeZoneID(tzID) {
  // Skip undefined zones.
  if (tzID === undefined) {
    return tzID;
  }

  // Special case handling (UTC, GMT).
  var upperID = tzID.toUpperCase();
  if (upperID === 'UTC' || upperID === 'GMT' ||
      upperID === 'ETC/UTC' || upperID === 'ETC/GMT') {
    return 'UTC';
  }

  // We expect only _ and / beside ASCII letters.
  // All inputs should conform to Area/Location from now on.
  var match = GetTimezoneNameCheckRE().exec(tzID);
  if (IS_NULL(match)) {
    throw new $RangeError('Expected Area/Location for time zone, got ' + tzID);
  }

  var result = toTitleCaseWord(match[1]) + '/' + toTitleCaseWord(match[2]);
  var i = 3;
  while (match[i] !== undefined && i < match.length) {
    result = result + '_' + toTitleCaseWord(match[i]);
    i++;
  }

  return result;
}

/**
 * Initializes the given object so it's a valid BreakIterator instance.
 * Useful for subclassing.
 */
function initializeBreakIterator(iterator, locales, options) {
  if (iterator.hasOwnProperty('__initializedIntlObject')) {
    throw new $TypeError('Trying to re-initialize v8BreakIterator object.');
  }

  if (options === undefined) {
    options = {};
  }

  var getOption = getGetOption(options, 'breakiterator');

  var internalOptions = {};

  defineWEProperty(internalOptions, 'type', getOption(
    'type', 'string', ['character', 'word', 'sentence', 'line'], 'word'));

  var locale = resolveLocale('breakiterator', locales, options);
  var resolved = $Object.defineProperties({}, {
    requestedLocale: {value: locale.locale, writable: true},
    type: {value: internalOptions.type, writable: true},
    locale: {writable: true}
  });

  var internalIterator = %CreateBreakIterator(locale.locale,
                                              internalOptions,
                                              resolved);

  $Object.defineProperty(iterator, 'iterator', {value: internalIterator});
  $Object.defineProperty(iterator, 'resolved', {value: resolved});
  $Object.defineProperty(iterator, '__initializedIntlObject',
                         {value: 'breakiterator'});

  return iterator;
}


/**
 * Constructs Intl.v8BreakIterator object given optional locales and options
 * parameters.
 *
 * @constructor
 */
%SetProperty(Intl, 'v8BreakIterator', function() {
    var locales = %_Arguments(0);
    var options = %_Arguments(1);

    if (!this || this === Intl) {
      // Constructor is called as a function.
      return new Intl.v8BreakIterator(locales, options);
    }

    return initializeBreakIterator(toObject(this), locales, options);
  },
  DONT_ENUM
);


/**
 * BreakIterator resolvedOptions method.
 */
%SetProperty(Intl.v8BreakIterator.prototype, 'resolvedOptions', function() {
    if (%_IsConstructCall()) {
      throw new $TypeError(ORDINARY_FUNCTION_CALLED_AS_CONSTRUCTOR);
    }

    if (!this || typeof this !== 'object' ||
        this.__initializedIntlObject !== 'breakiterator') {
      throw new $TypeError('resolvedOptions method called on a non-object or ' +
          'on a object that is not Intl.v8BreakIterator.');
    }

    var segmenter = this;
    var locale = getOptimalLanguageTag(segmenter.resolved.requestedLocale,
                                       segmenter.resolved.locale);

    return {
      locale: locale,
      type: segmenter.resolved.type
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
%SetProperty(Intl.v8BreakIterator, 'supportedLocalesOf', function(locales) {
    if (%_IsConstructCall()) {
      throw new $TypeError(ORDINARY_FUNCTION_CALLED_AS_CONSTRUCTOR);
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
  %BreakIteratorAdoptText(iterator.iterator, $String(text));
}


/**
 * Returns index of the first break in the string and moves current pointer.
 */
function first(iterator) {
  return %BreakIteratorFirst(iterator.iterator);
}


/**
 * Returns the index of the next break and moves the pointer.
 */
function next(iterator) {
  return %BreakIteratorNext(iterator.iterator);
}


/**
 * Returns index of the current break.
 */
function current(iterator) {
  return %BreakIteratorCurrent(iterator.iterator);
}


/**
 * Returns type of the current break.
 */
function breakType(iterator) {
  return %BreakIteratorBreakType(iterator.iterator);
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
  'collator': undefined,
  'numberformat': undefined,
  'dateformatall': undefined,
  'dateformatdate': undefined,
  'dateformattime': undefined,
};


/**
 * Returns cached or newly created instance of a given service.
 * We cache only default instances (where no locales or options are provided).
 */
function cachedOrNewService(service, locales, options, defaults) {
  var useOptions = (defaults === undefined) ? options : defaults;
  if (locales === undefined && options === undefined) {
    if (defaultObjects[service] === undefined) {
      defaultObjects[service] = new savedObjects[service](locales, useOptions);
    }
    return defaultObjects[service];
  }
  return new savedObjects[service](locales, useOptions);
}


/**
 * Compares this and that, and returns less than 0, 0 or greater than 0 value.
 * Overrides the built-in method.
 */
$Object.defineProperty($String.prototype, 'localeCompare', {
  value: function(that) {
    if (%_IsConstructCall()) {
      throw new $TypeError(ORDINARY_FUNCTION_CALLED_AS_CONSTRUCTOR);
    }

    if (IS_NULL_OR_UNDEFINED(this)) {
      throw new $TypeError('Method invoked on undefined or null value.');
    }

    var locales = %_Arguments(1);
    var options = %_Arguments(2);
    var collator = cachedOrNewService('collator', locales, options);
    return compare(collator, this, that);
  },
  writable: true,
  configurable: true,
  enumerable: false
});
%FunctionSetName($String.prototype.localeCompare, 'localeCompare');
%FunctionRemovePrototype($String.prototype.localeCompare);
%SetNativeFlag($String.prototype.localeCompare);


/**
 * Formats a Number object (this) using locale and options values.
 * If locale or options are omitted, defaults are used.
 */
$Object.defineProperty($Number.prototype, 'toLocaleString', {
  value: function() {
    if (%_IsConstructCall()) {
      throw new $TypeError(ORDINARY_FUNCTION_CALLED_AS_CONSTRUCTOR);
    }

    if (!(this instanceof $Number) && typeof(this) !== 'number') {
      throw new $TypeError('Method invoked on an object that is not Number.');
    }

    var locales = %_Arguments(0);
    var options = %_Arguments(1);
    var numberFormat = cachedOrNewService('numberformat', locales, options);
    return formatNumber(numberFormat, this);
  },
  writable: true,
  configurable: true,
  enumerable: false
});
%FunctionSetName($Number.prototype.toLocaleString, 'toLocaleString');
%FunctionRemovePrototype($Number.prototype.toLocaleString);
%SetNativeFlag($Number.prototype.toLocaleString);


/**
 * Returns actual formatted date or fails if date parameter is invalid.
 */
function toLocaleDateTime(date, locales, options, required, defaults, service) {
  if (!(date instanceof $Date)) {
    throw new $TypeError('Method invoked on an object that is not Date.');
  }

  if ($isNaN(date)) {
    return 'Invalid Date';
  }

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
$Object.defineProperty($Date.prototype, 'toLocaleString', {
  value: function() {
    if (%_IsConstructCall()) {
      throw new $TypeError(ORDINARY_FUNCTION_CALLED_AS_CONSTRUCTOR);
    }

    var locales = %_Arguments(0);
    var options = %_Arguments(1);
    return toLocaleDateTime(
        this, locales, options, 'any', 'all', 'dateformatall');
  },
  writable: true,
  configurable: true,
  enumerable: false
});
%FunctionSetName($Date.prototype.toLocaleString, 'toLocaleString');
%FunctionRemovePrototype($Date.prototype.toLocaleString);
%SetNativeFlag($Date.prototype.toLocaleString);


/**
 * Formats a Date object (this) using locale and options values.
 * If locale or options are omitted, defaults are used - only date is present
 * in the output.
 */
$Object.defineProperty($Date.prototype, 'toLocaleDateString', {
  value: function() {
    if (%_IsConstructCall()) {
      throw new $TypeError(ORDINARY_FUNCTION_CALLED_AS_CONSTRUCTOR);
    }

    var locales = %_Arguments(0);
    var options = %_Arguments(1);
    return toLocaleDateTime(
        this, locales, options, 'date', 'date', 'dateformatdate');
  },
  writable: true,
  configurable: true,
  enumerable: false
});
%FunctionSetName($Date.prototype.toLocaleDateString, 'toLocaleDateString');
%FunctionRemovePrototype($Date.prototype.toLocaleDateString);
%SetNativeFlag($Date.prototype.toLocaleDateString);


/**
 * Formats a Date object (this) using locale and options values.
 * If locale or options are omitted, defaults are used - only time is present
 * in the output.
 */
$Object.defineProperty($Date.prototype, 'toLocaleTimeString', {
  value: function() {
    if (%_IsConstructCall()) {
      throw new $TypeError(ORDINARY_FUNCTION_CALLED_AS_CONSTRUCTOR);
    }

    var locales = %_Arguments(0);
    var options = %_Arguments(1);
    return toLocaleDateTime(
        this, locales, options, 'time', 'time', 'dateformattime');
  },
  writable: true,
  configurable: true,
  enumerable: false
});
%FunctionSetName($Date.prototype.toLocaleTimeString, 'toLocaleTimeString');
%FunctionRemovePrototype($Date.prototype.toLocaleTimeString);
%SetNativeFlag($Date.prototype.toLocaleTimeString);

return Intl;
}())});
