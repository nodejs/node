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
Object.defineProperty(String.prototype, 'localeCompare', {
  value: function(that) {
    if (%_IsConstructCall()) {
      throw new TypeError(ORDINARY_FUNCTION_CALLED_AS_CONSTRUCTOR);
    }

    if (this === undefined || this === null) {
      throw new TypeError('Method invoked on undefined or null value.');
    }

    var locales = arguments[1];
    var options = arguments[2];
    var collator = cachedOrNewService('collator', locales, options);
    return compare(collator, this, that);
  },
  writable: true,
  configurable: true,
  enumerable: false
});
%FunctionSetName(String.prototype.localeCompare, 'localeCompare');
%FunctionRemovePrototype(String.prototype.localeCompare);
%SetNativeFlag(String.prototype.localeCompare);


/**
 * Formats a Number object (this) using locale and options values.
 * If locale or options are omitted, defaults are used.
 */
Object.defineProperty(Number.prototype, 'toLocaleString', {
  value: function() {
    if (%_IsConstructCall()) {
      throw new TypeError(ORDINARY_FUNCTION_CALLED_AS_CONSTRUCTOR);
    }

    if (!(this instanceof Number) && typeof(this) !== 'number') {
      throw new TypeError('Method invoked on an object that is not Number.');
    }

    var locales = arguments[0];
    var options = arguments[1];
    var numberFormat = cachedOrNewService('numberformat', locales, options);
    return formatNumber(numberFormat, this);
  },
  writable: true,
  configurable: true,
  enumerable: false
});
%FunctionSetName(Number.prototype.toLocaleString, 'toLocaleString');
%FunctionRemovePrototype(Number.prototype.toLocaleString);
%SetNativeFlag(Number.prototype.toLocaleString);


/**
 * Returns actual formatted date or fails if date parameter is invalid.
 */
function toLocaleDateTime(date, locales, options, required, defaults, service) {
  if (!(date instanceof Date)) {
    throw new TypeError('Method invoked on an object that is not Date.');
  }

  if (isNaN(date)) {
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
Object.defineProperty(Date.prototype, 'toLocaleString', {
  value: function() {
    if (%_IsConstructCall()) {
      throw new TypeError(ORDINARY_FUNCTION_CALLED_AS_CONSTRUCTOR);
    }

    var locales = arguments[0];
    var options = arguments[1];
    return toLocaleDateTime(
        this, locales, options, 'any', 'all', 'dateformatall');
  },
  writable: true,
  configurable: true,
  enumerable: false
});
%FunctionSetName(Date.prototype.toLocaleString, 'toLocaleString');
%FunctionRemovePrototype(Date.prototype.toLocaleString);
%SetNativeFlag(Date.prototype.toLocaleString);


/**
 * Formats a Date object (this) using locale and options values.
 * If locale or options are omitted, defaults are used - only date is present
 * in the output.
 */
Object.defineProperty(Date.prototype, 'toLocaleDateString', {
  value: function() {
    if (%_IsConstructCall()) {
      throw new TypeError(ORDINARY_FUNCTION_CALLED_AS_CONSTRUCTOR);
    }

    var locales = arguments[0];
    var options = arguments[1];
    return toLocaleDateTime(
        this, locales, options, 'date', 'date', 'dateformatdate');
  },
  writable: true,
  configurable: true,
  enumerable: false
});
%FunctionSetName(Date.prototype.toLocaleDateString, 'toLocaleDateString');
%FunctionRemovePrototype(Date.prototype.toLocaleDateString);
%SetNativeFlag(Date.prototype.toLocaleDateString);


/**
 * Formats a Date object (this) using locale and options values.
 * If locale or options are omitted, defaults are used - only time is present
 * in the output.
 */
Object.defineProperty(Date.prototype, 'toLocaleTimeString', {
  value: function() {
    if (%_IsConstructCall()) {
      throw new TypeError(ORDINARY_FUNCTION_CALLED_AS_CONSTRUCTOR);
    }

    var locales = arguments[0];
    var options = arguments[1];
    return toLocaleDateTime(
        this, locales, options, 'time', 'time', 'dateformattime');
  },
  writable: true,
  configurable: true,
  enumerable: false
});
%FunctionSetName(Date.prototype.toLocaleTimeString, 'toLocaleTimeString');
%FunctionRemovePrototype(Date.prototype.toLocaleTimeString);
%SetNativeFlag(Date.prototype.toLocaleTimeString);
