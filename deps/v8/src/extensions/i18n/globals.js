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


/**
 * List of available services.
 */
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
var UNICODE_EXTENSION_RE = new RegExp('-u(-[a-z0-9]{2,8})+', 'g');

/**
 * Matches any Unicode extension.
 */
var ANY_EXTENSION_RE = new RegExp('-[a-z0-9]{1}-.*', 'g');

/**
 * Replace quoted text (single quote, anything but the quote and quote again).
 */
var QUOTED_STRING_RE = new RegExp("'[^']+'", 'g');

/**
 * Matches valid service name.
 */
var SERVICE_RE =
    new RegExp('^(collator|numberformat|dateformat|breakiterator)$');

/**
 * Validates a language tag against bcp47 spec.
 * Actual value is assigned on first run.
 */
var LANGUAGE_TAG_RE = undefined;

/**
 * Helps find duplicate variants in the language tag.
 */
var LANGUAGE_VARIANT_RE = undefined;

/**
 * Helps find duplicate singletons in the language tag.
 */
var LANGUAGE_SINGLETON_RE = undefined;

/**
 * Matches valid IANA time zone names.
 */
var TIMEZONE_NAME_CHECK_RE =
    new RegExp('^([A-Za-z]+)/([A-Za-z]+)(?:_([A-Za-z]+))*$');

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
 * Object attributes (configurable, writable, enumerable).
 * To combine attributes, OR them.
 * Values/names are copied from v8/include/v8.h:PropertyAttribute
 */
var ATTRIBUTES = {
  'NONE': 0,
  'READ_ONLY': 1,
  'DONT_ENUM': 2,
  'DONT_DELETE': 4
};

/**
 * Error message for when function object is created with new and it's not
 * a constructor.
 */
var ORDINARY_FUNCTION_CALLED_AS_CONSTRUCTOR =
  'Function object that\'s not a constructor was created with new';
