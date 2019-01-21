'use strict';
// Value of https://w3c.github.io/webappsec-subresource-integrity/#the-integrity-attribute

// Returns [{algorithm, value (in base64 string), options,}]
const {
  ERR_SRI_PARSE
} = require('internal/errors').codes;
const kWSP = '[\\x20\\x09]';
const kVCHAR = '[\\x21-\\x7E]';
const kHASH_ALGO = 'sha256|sha384|sha512';
// Base64
const kHASH_VALUE = '[A-Za-z0-9+/]+[=]{0,2}';
const kHASH_EXPRESSION = `(${kHASH_ALGO})-(${kHASH_VALUE})`;
const kOPTION_EXPRESSION = `(${kVCHAR}*)`;
const kHASH_WITH_OPTIONS = `${kHASH_EXPRESSION}(?:[?](${kOPTION_EXPRESSION}))?`;
const kSRIPattern = new RegExp(`(${kWSP}*)(?:${kHASH_WITH_OPTIONS})`, 'g');
const { freeze } = Object;
Object.seal(kSRIPattern);
const kAllWSP = new RegExp(`^${kWSP}*$`);
Object.seal(kAllWSP);

const RegExpExec = Function.call.bind(RegExp.prototype.exec);
const RegExpTest = Function.call.bind(RegExp.prototype.test);
const StringSlice = Function.call.bind(String.prototype.slice);

const BufferFrom = require('buffer').Buffer.from;
const { defineProperty } = Object;

const parse = (str) => {
  kSRIPattern.lastIndex = 0;
  let prevIndex = 0;
  let match;
  const entries = [];
  while (match = RegExpExec(kSRIPattern, str)) {
    if (match.index !== prevIndex) {
      throw new ERR_SRI_PARSE(str, prevIndex);
    }
    if (entries.length > 0 && match[1] === '') {
      throw new ERR_SRI_PARSE(str, prevIndex);
    }

    // Avoid setters being fired
    defineProperty(entries, entries.length, {
      enumerable: true,
      configurable: true,
      value: freeze({
        __proto__: null,
        algorithm: match[2],
        value: BufferFrom(match[3], 'base64'),
        options: match[4] === undefined ? null : match[4],
      })
    });
    prevIndex = prevIndex + match[0].length;
  }

  if (prevIndex !== str.length) {
    if (!RegExpTest(kAllWSP, StringSlice(str, prevIndex))) {
      throw new ERR_SRI_PARSE(str, prevIndex);
    }
  }
  return entries;
};

module.exports = {
  parse,
};
