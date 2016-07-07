/*!
 * XRegExp Unicode Base 3.1.1
 * <xregexp.com>
 * Steven Levithan (c) 2008-2016 MIT License
 */

module.exports = function(XRegExp) {
    'use strict';

    /**
     * Adds base support for Unicode matching:
     * - Adds syntax `\p{..}` for matching Unicode tokens. Tokens can be inverted using `\P{..}` or
     *   `\p{^..}`. Token names ignore case, spaces, hyphens, and underscores. You can omit the
     *   braces for token names that are a single letter (e.g. `\pL` or `PL`).
     * - Adds flag A (astral), which enables 21-bit Unicode support.
     * - Adds the `XRegExp.addUnicodeData` method used by other addons to provide character data.
     *
     * Unicode Base relies on externally provided Unicode character data. Official addons are
     * available to provide data for Unicode categories, scripts, blocks, and properties.
     *
     * @requires XRegExp
     */

    // ==--------------------------==
    // Private stuff
    // ==--------------------------==

    // Storage for Unicode data
    var unicode = {};

    // Reuse utils
    var dec = XRegExp._dec;
    var hex = XRegExp._hex;
    var pad4 = XRegExp._pad4;

    // Generates a token lookup name: lowercase, with hyphens, spaces, and underscores removed
    function normalize(name) {
        return name.replace(/[- _]+/g, '').toLowerCase();
    }

    // Gets the decimal code of a literal code unit, \xHH, \uHHHH, or a backslash-escaped literal
    function charCode(chr) {
        var esc = /^\\[xu](.+)/.exec(chr);
        return esc ?
            dec(esc[1]) :
            chr.charCodeAt(chr.charAt(0) === '\\' ? 1 : 0);
    }

    // Inverts a list of ordered BMP characters and ranges
    function invertBmp(range) {
        var output = '';
        var lastEnd = -1;
        XRegExp.forEach(
            range,
            /(\\x..|\\u....|\\?[\s\S])(?:-(\\x..|\\u....|\\?[\s\S]))?/,
            function(m) {
                var start = charCode(m[1]);
                if (start > (lastEnd + 1)) {
                    output += '\\u' + pad4(hex(lastEnd + 1));
                    if (start > (lastEnd + 2)) {
                        output += '-\\u' + pad4(hex(start - 1));
                    }
                }
                lastEnd = charCode(m[2] || m[1]);
            }
        );
        if (lastEnd < 0xFFFF) {
            output += '\\u' + pad4(hex(lastEnd + 1));
            if (lastEnd < 0xFFFE) {
                output += '-\\uFFFF';
            }
        }
        return output;
    }

    // Generates an inverted BMP range on first use
    function cacheInvertedBmp(slug) {
        var prop = 'b!';
        return unicode[slug][prop] || (
            unicode[slug][prop] = invertBmp(unicode[slug].bmp)
        );
    }

    // Combines and optionally negates BMP and astral data
    function buildAstral(slug, isNegated) {
        var item = unicode[slug],
            combined = '';
        if (item.bmp && !item.isBmpLast) {
            combined = '[' + item.bmp + ']' + (item.astral ? '|' : '');
        }
        if (item.astral) {
            combined += item.astral;
        }
        if (item.isBmpLast && item.bmp) {
            combined += (item.astral ? '|' : '') + '[' + item.bmp + ']';
        }
        // Astral Unicode tokens always match a code point, never a code unit
        return isNegated ?
            '(?:(?!' + combined + ')(?:[\uD800-\uDBFF][\uDC00-\uDFFF]|[\0-\uFFFF]))' :
            '(?:' + combined + ')';
    }

    // Builds a complete astral pattern on first use
    function cacheAstral(slug, isNegated) {
        var prop = isNegated ? 'a!' : 'a=';
        return unicode[slug][prop] || (
            unicode[slug][prop] = buildAstral(slug, isNegated)
        );
    }

    // ==--------------------------==
    // Core functionality
    // ==--------------------------==

    /*
     * Add Unicode token syntax: \p{..}, \P{..}, \p{^..}. Also add astral mode (flag A).
     */
    XRegExp.addToken(
        // Use `*` instead of `+` to avoid capturing `^` as the token name in `\p{^}`
        /\\([pP])(?:{(\^?)([^}]*)}|([A-Za-z]))/,
        function(match, scope, flags) {
            var ERR_DOUBLE_NEG = 'Invalid double negation ',
                ERR_UNKNOWN_NAME = 'Unknown Unicode token ',
                ERR_UNKNOWN_REF = 'Unicode token missing data ',
                ERR_ASTRAL_ONLY = 'Astral mode required for Unicode token ',
                ERR_ASTRAL_IN_CLASS = 'Astral mode does not support Unicode tokens within character classes',
                // Negated via \P{..} or \p{^..}
                isNegated = match[1] === 'P' || !!match[2],
                // Switch from BMP (0-FFFF) to astral (0-10FFFF) mode via flag A
                isAstralMode = flags.indexOf('A') > -1,
                // Token lookup name. Check `[4]` first to avoid passing `undefined` via `\p{}`
                slug = normalize(match[4] || match[3]),
                // Token data object
                item = unicode[slug];

            if (match[1] === 'P' && match[2]) {
                throw new SyntaxError(ERR_DOUBLE_NEG + match[0]);
            }
            if (!unicode.hasOwnProperty(slug)) {
                throw new SyntaxError(ERR_UNKNOWN_NAME + match[0]);
            }

            // Switch to the negated form of the referenced Unicode token
            if (item.inverseOf) {
                slug = normalize(item.inverseOf);
                if (!unicode.hasOwnProperty(slug)) {
                    throw new ReferenceError(ERR_UNKNOWN_REF + match[0] + ' -> ' + item.inverseOf);
                }
                item = unicode[slug];
                isNegated = !isNegated;
            }

            if (!(item.bmp || isAstralMode)) {
                throw new SyntaxError(ERR_ASTRAL_ONLY + match[0]);
            }
            if (isAstralMode) {
                if (scope === 'class') {
                    throw new SyntaxError(ERR_ASTRAL_IN_CLASS);
                }

                return cacheAstral(slug, isNegated);
            }

            return scope === 'class' ?
                (isNegated ? cacheInvertedBmp(slug) : item.bmp) :
                (isNegated ? '[^' : '[') + item.bmp + ']';
        },
        {
            scope: 'all',
            optionalFlags: 'A',
            leadChar: '\\'
        }
    );

    /**
     * Adds to the list of Unicode tokens that XRegExp regexes can match via `\p` or `\P`.
     *
     * @param {Array} data Objects with named character ranges. Each object may have properties
     *   `name`, `alias`, `isBmpLast`, `inverseOf`, `bmp`, and `astral`. All but `name` are
     *   optional, although one of `bmp` or `astral` is required (unless `inverseOf` is set). If
     *   `astral` is absent, the `bmp` data is used for BMP and astral modes. If `bmp` is absent,
     *   the name errors in BMP mode but works in astral mode. If both `bmp` and `astral` are
     *   provided, the `bmp` data only is used in BMP mode, and the combination of `bmp` and
     *   `astral` data is used in astral mode. `isBmpLast` is needed when a token matches orphan
     *   high surrogates *and* uses surrogate pairs to match astral code points. The `bmp` and
     *   `astral` data should be a combination of literal characters and `\xHH` or `\uHHHH` escape
     *   sequences, with hyphens to create ranges. Any regex metacharacters in the data should be
     *   escaped, apart from range-creating hyphens. The `astral` data can additionally use
     *   character classes and alternation, and should use surrogate pairs to represent astral code
     *   points. `inverseOf` can be used to avoid duplicating character data if a Unicode token is
     *   defined as the exact inverse of another token.
     * @example
     *
     * // Basic use
     * XRegExp.addUnicodeData([{
     *   name: 'XDigit',
     *   alias: 'Hexadecimal',
     *   bmp: '0-9A-Fa-f'
     * }]);
     * XRegExp('\\p{XDigit}:\\p{Hexadecimal}+').test('0:3D'); // -> true
     */
    XRegExp.addUnicodeData = function(data) {
        var ERR_NO_NAME = 'Unicode token requires name',
            ERR_NO_DATA = 'Unicode token has no character data ',
            item,
            i;

        for (i = 0; i < data.length; ++i) {
            item = data[i];
            if (!item.name) {
                throw new Error(ERR_NO_NAME);
            }
            if (!(item.inverseOf || item.bmp || item.astral)) {
                throw new Error(ERR_NO_DATA + item.name);
            }
            unicode[normalize(item.name)] = item;
            if (item.alias) {
                unicode[normalize(item.alias)] = item;
            }
        }

        // Reset the pattern cache used by the `XRegExp` constructor, since the same pattern and
        // flags might now produce different results
        XRegExp.cache.flush('patterns');
    };

};
