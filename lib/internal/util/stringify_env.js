'use strict';

const {
    ArrayPrototypePush,
    ObjectEntries,
    RegExpPrototypeTest,
    String,
    StringPrototypeReplace,
} = primordials;

const {
    validateObject,
} = require('internal/validators');

const kEnvNeedQuoteRegExp = /[\s"'\n#=]/;
const kEnvEscapeRegExp = /(["\\])/g;
const kEnvNewlinesRegExp = /\n/g;
const kEnvReturnRegExp = /\r/g;

function stringifyEnv(env) {
    validateObject(env, 'env');

    const lines = [];
    const entries = ObjectEntries(env);

    for (let i = 0; i < entries.length; i++) {
        const { 0: key, 1: value } = entries[i];
        const strValue = String(value);

        // If the value contains characters that need quoting in .env files
        // (space, newline, quote, etc.), quoting is safer.
        // For simplicity and safety, we quote if it has spaces, quotes, newlines,
        // or starts with # (comment).
        if (strValue === '' || RegExpPrototypeTest(kEnvNeedQuoteRegExp, strValue)) {
            // Escape existing double quotes and newlines
            const escaped = StringPrototypeReplace(strValue, kEnvEscapeRegExp, '\\$1')
                .replace(kEnvNewlinesRegExp, '\\n')
                .replace(kEnvReturnRegExp, '\\r');
            ArrayPrototypePush(lines, `${key}="${escaped}"`);
        } else {
            ArrayPrototypePush(lines, `${key}=${strValue}`);
        }
    }

    return lines.join('\n');
}

module.exports = {
    stringifyEnv,
};
