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
 * Adds bound method to the prototype of the given object.
 */
function addBoundMethod(obj, methodName, implementation, length) {
  function getter() {
    if (!this || typeof this !== 'object' ||
        this.__initializedIntlObject === undefined) {
        throw new TypeError('Method ' + methodName + ' called on a ' +
                            'non-object or on a wrong type of object.');
    }
    var internalName = '__bound' + methodName + '__';
    if (this[internalName] === undefined) {
      var that = this;
      var boundMethod;
      if (length === undefined || length === 2) {
        boundMethod = function(x, y) {
          if (%_IsConstructCall()) {
            throw new TypeError(ORDINARY_FUNCTION_CALLED_AS_CONSTRUCTOR);
          }
          return implementation(that, x, y);
        }
      } else if (length === 1) {
        boundMethod = function(x) {
          if (%_IsConstructCall()) {
            throw new TypeError(ORDINARY_FUNCTION_CALLED_AS_CONSTRUCTOR);
          }
          return implementation(that, x);
        }
      } else {
        boundMethod = function() {
          if (%_IsConstructCall()) {
            throw new TypeError(ORDINARY_FUNCTION_CALLED_AS_CONSTRUCTOR);
          }
          // DateTimeFormat.format needs to be 0 arg method, but can stil
          // receive optional dateValue param. If one was provided, pass it
          // along.
          if (arguments.length > 0) {
            return implementation(that, arguments[0]);
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

  Object.defineProperty(obj.prototype, methodName, {
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
  if (service.match(SERVICE_RE) === null) {
    throw new Error('Internal error, wrong service type: ' + service);
  }

  // Provide defaults if matcher was not specified.
  if (options === undefined) {
    options = {};
  } else {
    options = toObject(options);
  }

  var matcher = options.localeMatcher;
  if (matcher !== undefined) {
    matcher = String(matcher);
    if (matcher !== 'lookup' && matcher !== 'best fit') {
      throw new RangeError('Illegal value for localeMatcher:' + matcher);
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
    var locale = requestedLocales[i].replace(UNICODE_EXTENSION_RE, '');
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
    throw new Error('Internal ' + caller + ' error. ' +
                    'Default options are missing.');
  }

  var getOption = function getOption(property, type, values, defaultValue) {
    if (options[property] !== undefined) {
      var value = options[property];
      switch (type) {
        case 'boolean':
          value = Boolean(value);
          break;
        case 'string':
          value = String(value);
          break;
        case 'number':
          value = Number(value);
          break;
        default:
          throw new Error('Internal error. Wrong value type.');
      }
      if (values !== undefined && values.indexOf(value) === -1) {
        throw new RangeError('Value ' + value + ' out of range for ' + caller +
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
  native function NativeJSGetDefaultICULocale();

  if (service.match(SERVICE_RE) === null) {
    throw new Error('Internal error, wrong service type: ' + service);
  }

  // Cache these, they don't ever change per service.
  if (AVAILABLE_LOCALES[service] === undefined) {
    AVAILABLE_LOCALES[service] = getAvailableLocalesOf(service);
  }

  for (var i = 0; i < requestedLocales.length; ++i) {
    // Remove all extensions.
    var locale = requestedLocales[i].replace(ANY_EXTENSION_RE, '');
    do {
      if (AVAILABLE_LOCALES[service][locale] !== undefined) {
        // Return the resolved locale and extension.
        var extensionMatch = requestedLocales[i].match(UNICODE_EXTENSION_RE);
        var extension = (extensionMatch === null) ? '' : extensionMatch[0];
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
    DEFAULT_ICU_LOCALE = NativeJSGetDefaultICULocale();
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
  if (value === undefined || value === null) {
    throw new TypeError('Value cannot be converted to an Object.');
  }

  return Object(value);
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
    return '-' + key + '-' + String(value);
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
    Object.defineProperty(array, index, {value: element,
                                         configurable: false,
                                         writable: false,
                                         enumerable: true});
  });

  Object.defineProperty(array, 'length', {value: array.length,
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
  native function NativeJSGetLanguageTagVariants();

  // Take care of grandfathered or simple cases.
  if (original === resolved) {
    return original;
  }

  var locales = NativeJSGetLanguageTagVariants([original, resolved]);
  if (locales[0].maximized !== locales[1].maximized) {
    return resolved;
  }

  // Preserve extensions of resolved locale, but swap base tags with original.
  var resolvedBase = new RegExp('^' + locales[1].base);
  return resolved.replace(resolvedBase, locales[0].base);
}


/**
 * Returns an Object that contains all of supported locales for a given
 * service.
 * In addition to the supported locales we add xx-ZZ locale for each xx-Yyyy-ZZ
 * that is supported. This is required by the spec.
 */
function getAvailableLocalesOf(service) {
  native function NativeJSAvailableLocalesOf();
  var available = NativeJSAvailableLocalesOf(service);

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
  Object.defineProperty(object, property,
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
  Object.defineProperty(object, property,
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
