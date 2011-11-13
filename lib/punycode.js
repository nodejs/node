/*!
 * Punycode.js <http://mths.be/punycode>
 * Copyright 2011 Mathias Bynens <http://mathiasbynens.be/>
 * Available under MIT license <http://mths.be/mit>
 */

;(function(window) {

	/**
	 * The `Punycode` object.
	 * @name Punycode
	 * @type Object
	 */
	var Punycode,

	/** Detect free variables `define`, `exports`, and `require` */
	freeDefine = typeof define == 'function' && typeof define.amd == 'object' &&
		define.amd && define,
	freeExports = typeof exports == 'object' && exports &&
		(typeof global == 'object' && global && global == global.global &&
		(window = global), exports),
	freeRequire = typeof require == 'function' && require,

	/** Highest positive signed 32-bit float value */
	maxInt = 2147483647, // aka. 0x7FFFFFFF or 2^31-1

	/** Bootstring parameters */
	base = 36,
	tMin = 1,
	tMax = 26,
	skew = 38,
	damp = 700,
	initialBias = 72,
	initialN = 128, // 0x80
	delimiter = '-', // '\x2D'

	/** Regular expressions */
	regexASCII = /[^\x20-\x7e]/,
	regexPunycode = /^xn--/,

	/** Error messages */
	errors = {
		'overflow': 'Overflow: input needs wider integers to process.',
		'utf16decode': 'UTF-16(decode): illegal UTF-16 sequence',
		'utf16encode': 'UTF-16(encode): illegal UTF-16 value',
		'not-basic': 'Illegal input >= 0x80 (not a basic code point)',
		'invalid-input': 'Invalid input'
	},

	/** Convenience shortcuts */
	baseMinusTMin = base - tMin,
	floor = Math.floor,
	stringFromCharCode = String.fromCharCode;

	/*--------------------------------------------------------------------------*/

	/**
	 * A generic error utility function.
	 * @private
	 * @param {String} type The error type.
	 * @returns {Error} Throws a `RangeError` with the applicable error message.
	 */
	function error(type) {
		throw RangeError(errors[type]);
	}

	/**
	 * A generic `Array#map` utility function.
	 * @private
	 * @param {Array} array The array to iterate over.
	 * @param {Function} callback The function that gets called for every array
	 * item.
	 * @returns {Array} A new array of values returned by the callback function.
	 */
	function map(array, fn) {
		var length = array.length;
		while (length--) {
			array[length] = fn(array[length]);
		}
		return array;
	}

	/**
	 * A simple `Array#map`-like wrapper to work with domain name strings.
	 * @private
	 * @param {String} domain The domain name.
	 * @param {Function} callback The function that gets called for every
	 * character.
	 * @returns {Array} A new string of characters returned by the callback
	 * function.
	 */
	function mapDomain(string, fn) {
		var glue = '.';
		return map(string.split(glue), fn).join(glue);
	}

	/**
	 * Creates an array containing the decimal code points of each character in
	 * the string.
	 * @see `Punycode.utf16.encode`
	 * @memberOf Punycode.utf16
	 * @name decode
	 * @param {String} string The Unicode input string.
	 * @returns {Array} The new array.
	 */
	function utf16decode(string) {
		var output = [],
		    counter = 0,
		    length = string.length,
		    value,
		    extra;
		while (counter < length) {
			value = string.charCodeAt(counter++);
			if ((value & 0xF800) == 0xD800) {
				extra = string.charCodeAt(counter++);
				if ((value & 0xFC00) != 0xD800 || (extra & 0xFC00) != 0xDC00) {
					error('utf16decode');
				}
				value = ((value & 0x3FF) << 10) + (extra & 0x3FF) + 0x10000;
			}
			output.push(value);
		}
		return output;
	}

	/**
	 * Creates a string based on an array of decimal code points.
	 * @see `Punycode.utf16.decode`
	 * @memberOf Punycode.utf16
	 * @name encode
	 * @param {Array} codePoints The array of decimal code points.
	 * @returns {String} The new string.
	 */
	function utf16encode(array) {
		return map(array, function(value) {
			var output = '';
			if ((value & 0xF800) == 0xD800) {
				error('utf16encode');
			}
			if (value > 0xFFFF) {
				value -= 0x10000;
				output += stringFromCharCode(value >>> 10 & 0x3FF | 0xD800);
				value = 0xDC00 | value & 0x3FF;
			}
			output += stringFromCharCode(value);
			return output;
		}).join('');
	}

	/**
	 * Converts a basic code point into a digit/integer.
	 * @see `digitToBasic()`
	 * @private
	 * @param {Number} codePoint The basic (decimal) code point.
	 * @returns {Number} The numeric value of a basic code point (for use in
	 * representing integers) in the range `0` to `base - 1`, or `base` if
	 * the code point does not represent a value.
	 */
	function basicToDigit(codePoint) {
		return codePoint - 48 < 10
			? codePoint - 22
			: codePoint - 65 < 26
				? codePoint - 65
				: codePoint - 97 < 26
					? codePoint - 97
					: base;
	}

	/**
	 * Converts a digit/integer into a basic code point.
	 * @see `basicToDigit()`
	 * @private
	 * @param {Number} digit The numeric value of a basic code point.
	 * @returns {Number} The basic code point whose value (when used for
	 * representing integers) is `digit`, which needs to be in the range
	 * `0` to `base - 1`. If `flag` is non-zero, the uppercase form is
	 * used; else, the lowercase form is used. The behavior is undefined
	 * if flag is non-zero and `digit` has no uppercase form.
	 */
	function digitToBasic(digit, flag) {
		//  0..25 map to ASCII a..z or A..Z
		// 26..35 map to ASCII 0..9
		return digit + 22 + 75 * (digit < 26) - ((flag != 0) << 5);
	}

	/**
	 * Bias adaptation function as per section 3.4 of RFC 3492.
	 * http://tools.ietf.org/html/rfc3492#section-3.4
	 * @private
	 */
	function adapt(delta, numPoints, firstTime) {
		var k = 0;
		delta = firstTime ? floor(delta / damp) : delta >> 1;
		delta += floor(delta / numPoints);
		for (/* no initialization */; delta > baseMinusTMin * tMax >> 1; k += base) {
			delta = floor(delta / baseMinusTMin);
		}
		return floor(k + (baseMinusTMin + 1) * delta / (delta + skew));
	}

	/**
	 * Converts a basic code point to lowercase is `flag` is falsy, or to
	 * uppercase if `flag` is truthy. The code point is unchanged if it's
	 * caseless. The behavior is undefined if `codePoint` is not a basic code
	 * point.
	 * @private
	 * @param {Number} codePoint The numeric value of a basic code point.
	 * @returns {Number} The resulting basic code point.
	 */
	function encodeBasic(codePoint, flag) {
		codePoint -= (codePoint - 97 < 26) << 5;
		return codePoint + (!flag && codePoint - 65 < 26) << 5;
	}

	/**
	 * Converts a Punycode string of ASCII code points to a string of Unicode
	 * code points.
	 * @memberOf Punycode
	 * @param {String} input The Punycode string of ASCII code points.
	 * @param {Boolean} preserveCase A boolean value indicating if character
	 * case should be preserved or not.
	 * @returns {String} The resulting string of Unicode code points.
	 */
	function decode(input, preserveCase) {
		// Don't use UTF-16
		var output = [],
		    /**
		     * The `caseFlags` array needs room for at least `output.length` values,
		     * or it can be `undefined` if the case information is not needed. A
		     * truthy value suggests that the corresponding Unicode character be
		     * forced to uppercase (if possible), while falsy values suggest that it
		     * be forced to lowercase (if possible). ASCII code points are output
		     * already in the proper case, but their flags will be set appropriately
		     * so that applying the flags would be harmless.
		     */
		    caseFlags = [],
		    inputLength = input.length,
		    out,
		    i = 0,
		    n = initialN,
		    bias = initialBias,
		    basic,
		    j,
		    index,
		    oldi,
		    w,
		    k,
		    digit,
		    t,
		    length,
		    /** Cached calculation results */
		    baseMinusT;

		// Handle the basic code points: let `basic` be the number of input code
		// points before the last delimiter, or `0` if there is none, then copy
		// the first basic code points to the output.

		basic = input.lastIndexOf(delimiter);
		if (basic < 0) {
			basic = 0;
		}

		for (j = 0; j < basic; ++j) {
			if (preserveCase) {
				caseFlags[output.length] = input.charCodeAt(j) - 65 < 26;
			}
			// if it's not a basic code point
			if (input.charCodeAt(j) >= 0x80) {
				error('not-basic');
			}
			output.push(input.charCodeAt(j));
		}

		// Main decoding loop: start just after the last delimiter if any basic
		// code points were copied; start at the beginning otherwise.

		for (index = basic > 0 ? basic + 1 : 0; index < inputLength; /* no final expression */) {

			// `index` is the index of the next character to be consumed.
			// Decode a generalized variable-length integer into `delta`,
			// which gets added to `i`. The overflow checking is easier
			// if we increase `i` as we go, then subtract off its starting
			// value at the end to obtain `delta`.
			for (oldi = i, w = 1, k = base; /* no condition */; k += base) {

				if (index >= inputLength) {
					error('invalid-input');
				}

				digit = basicToDigit(input.charCodeAt(index++));

				if (digit >= base || digit > floor((maxInt - i) / w)) {
					error('overflow');
				}

				i += digit * w;
				t = k <= bias ? tMin : k >= bias + tMax ? tMax : k - bias;

				if (digit < t) {
					break;
				}

				baseMinusT = base - t;
				if (w > floor(maxInt / baseMinusT)) {
					error('overflow');
				}

				w *= baseMinusT;

			}

			out = output.length + 1;
			bias = adapt(i - oldi, out, oldi == 0);

			// `i` was supposed to wrap around from `out` to `0`,
			// incrementing `n` each time, so we'll fix that now:
			if (floor(i / out) > maxInt - n) {
				error('overflow');
			}

			n += floor(i / out);
			i %= out;

			// Insert `n` at position `i` of the output
			// The case of the last character determines `uppercase` flag
			if (preserveCase) {
				caseFlags.splice(i, 0, input.charCodeAt(index - 1) - 65 < 26);
			}

			output.splice(i, 0, n);
			i++;

		}

		if (preserveCase) {
			for (i = 0, length = output.length; i < length; i++) {
				if (caseFlags[i]) {
					output[i] = (stringFromCharCode(output[i]).toUpperCase()).charCodeAt(0);
				}
			}
		}
		return utf16encode(output);
	}

	/**
	 * Converts a string of Unicode code points to a Punycode string of ASCII
	 * code points.
	 * @memberOf Punycode
	 * @param {String} input The string of Unicode code points.
	 * @param {Boolean} preserveCase A boolean value indicating if character
	 * case should be preserved or not.
	 * @returns {String} The resulting Punycode string of ASCII code points.
	 */
	function encode(input, preserveCase) {
		var n,
		    delta,
		    handledCPCount,
		    basicLength,
		    bias,
		    j,
		    m,
		    q,
		    k,
		    t,
		    currentValue,
		    /**
		     * The `caseFlags` array will hold `inputLength` boolean values, where
		     * `true` suggests that the corresponding Unicode character be forced
		     * to uppercase after being decoded (if possible), and `false`
		     * suggests that it be forced to lowercase (if possible). ASCII code
		     * points are encoded literally, except that ASCII letters are forced
		     * to uppercase or lowercase according to the corresponding uppercase
		     * flags. If `caseFlags` remains `undefined` then ASCII letters are
		     * left as they are, and other code points are treated as if their
		     * uppercase flags were `true`.
		     */
		    caseFlags,
		    output = [],
		    /** `inputLength` will hold the number of code points in `input`. */
		    inputLength,
		    /** Cached calculation results */
		    handledCPCountPlusOne,
		    baseMinusT,
		    qMinusT;

		if (preserveCase) {
			// Preserve case, step 1 of 2: get a list of the unaltered string
			caseFlags = utf16decode(input);
		}

		// Convert the input in UTF-16 to Unicode
		input = utf16decode(input);

		// Cache the length
		inputLength = input.length;

		if (preserveCase) {
			// Preserve case, step 2 of 2: modify the list to true/false
			for (j = 0; j < inputLength; j++) {
				caseFlags[j] = input[j] != caseFlags[j];
			}
		}

		// Initialize the state
		n = initialN;
		delta = 0;
		bias = initialBias;

		// Handle the basic code points
		for (j = 0; j < inputLength; ++j) {
			currentValue = input[j];
			if (currentValue < 0x80) {
				output.push(
					stringFromCharCode(
						caseFlags ? encodeBasic(currentValue, caseFlags[j]) : currentValue
					)
				);
			}
		}

		handledCPCount = basicLength = output.length;

		// `handledCPCount` is the number of code points that have been handled;
		// `basicLength` is the number of basic code points.

		// Finish the basic string - if it is not empty - with a delimiter
		if (basicLength) {
			output.push(delimiter);
		}

		// Main encoding loop:
		while (handledCPCount < inputLength) {

			// All non-basic code points < n have been handled already. Find the next
			// larger one:

			for (m = maxInt, j = 0; j < inputLength; ++j) {
				currentValue = input[j];
				if (currentValue >= n && currentValue < m) {
					m = currentValue;
				}
			}

			// Increase `delta` enough to advance the decoder's <n,i> state to <m,0>,
			// but guard against overflow
			handledCPCountPlusOne = handledCPCount + 1;
			if (m - n > floor((maxInt - delta) / handledCPCountPlusOne)) {
				error('overflow');
			}

			delta += (m - n) * handledCPCountPlusOne;
			n = m;

			for (j = 0; j < inputLength; ++j) {
				currentValue = input[j];

				if (currentValue < n && ++delta > maxInt) {
					error('overflow');
				}

				if (currentValue == n) {
					// Represent delta as a generalized variable-length integer
					for (q = delta, k = base; /* no condition */; k += base) {
						t = k <= bias ? tMin : k >= bias + tMax ? tMax : k - bias;
						if (q < t) {
							break;
						}
						qMinusT = q - t;
						baseMinusT = base - t;
						output.push(
							stringFromCharCode(digitToBasic(t + qMinusT % baseMinusT, 0))
						);
						q = floor(qMinusT / baseMinusT);
					}

					output.push(stringFromCharCode(digitToBasic(q, preserveCase && caseFlags[j] ? 1 : 0)));
					bias = adapt(delta, handledCPCountPlusOne, handledCPCount == basicLength);
					delta = 0;
					++handledCPCount;
				}
			}

			++delta;
			++n;

		}
		return output.join('');
	}

	/**
	 * Converts a Punycode string representing a domain name to Unicode. Only the
	 * Punycoded parts of the domain name will be converted, i.e. it doesn't
	 * matter if you call it on a string that has already been converted to
	 * Unicode.
	 * @memberOf Punycode
	 * @param {String} domain The Punycode domain name to convert to Unicode.
	 * @returns {String} The Unicode representation of the given Punycode
	 * string.
	 */
	function toUnicode(domain) {
		return mapDomain(domain, function(string) {
			return regexPunycode.test(string)
				? decode(string.slice(4).toLowerCase())
				: string;
		});
	}

	/**
	 * Converts a Unicode string representing a domain name to Punycode. Only the
	 * non-ASCII parts of the domain name will be converted, i.e. it doesn't
	 * matter if you call it with a domain that's already in ASCII.
	 * @memberOf Punycode
	 * @param {String} domain The domain name to convert, as a Unicode string.
	 * @returns {String} The Punycode representation of the given domain name.
	 */
	function toASCII(domain) {
		return mapDomain(domain, function(string) {
			return regexASCII.test(string)
				? 'xn--' + encode(string)
				: string;
		});
	}

	/*--------------------------------------------------------------------------*/

	/** Define the public API */
	Punycode = {
		'version': '0.1.1',
		/**
		 * An object of methods to convert from JavaScript's internal character
		 * representation to Unicode and back.
		 * @memberOf Punycode
		 * @type Object
		 */
		'utf16': {
			'decode': utf16decode,
			'encode': utf16encode
		},
		'decode': decode,
		'encode': encode,
		'toASCII': toASCII,
		'toUnicode': toUnicode
	};

	/** Expose Punycode */
	if (freeExports) {
		if (typeof module == 'object' && module && module.exports == freeExports) {
			// in Node.js
			module.exports = Punycode;
		} else {
			// in Narwhal or Ringo
			freeExports.Punycode = Punycode;
		}
	} else if (freeDefine) {
		// via curl.js or RequireJS
		freeDefine(function() {
			return Punycode;
		});
	} else {
		// in a browser or Rhino
		window.Punycode = Punycode;
	}

}(this));
