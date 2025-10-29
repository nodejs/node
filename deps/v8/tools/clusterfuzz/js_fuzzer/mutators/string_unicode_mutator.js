// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview String Unicode mutator.
 * This mutator randomly replaces characters in identifiers, string literals,
 * and regex patterns with their corresponding \uXXXX Unicode escape sequence.
 * E.g., "a" becomes "\u0061"
 */

'use strict';

const babelTypes = require('@babel/types');
const mutator = require('./mutator.js');
const random = require('../random.js');

function toUnicodeEscape(char, options = {}) {
  const { allowX = true } = options;

  const codePoint = char.codePointAt(0);
  const hex = codePoint.toString(16);

  if (codePoint > 0xFFFF) {
    return `\\u{${hex}}`;
  }

  if (codePoint <= 0xFF && allowX) {
    const rand = Math.random();
    if (rand < 0.33) {
      return '\\x' + hex.padStart(2, '0');
    } else if (rand < 0.66) {
      return `\\u{${hex}}`;
    } else {
      return '\\u' + hex.padStart(4, '0');
    }
  }

  if (Math.random() < 0.5) {
    return `\\u{${hex}}`;
  } else {
    return '\\u' + hex.padStart(4, '0');
  }
}

class StringUnicodeMutator extends mutator.Mutator {
  get visitor() {
    const settings = this.settings;

    const shouldMutateIdentifiers = random.choose(settings.ENABLE_IDENTIFIER_UNICODE_ESCAPE);
    const shouldMutateStringLiterals = random.choose(settings.ENABLE_STRINGLITERAL_UNICODE_ESCAPE);
    const shouldMutateRegExpLiterals = random.choose(settings.ENABLE_REGEXPLITERAL_UNICODE_ESCAPE);

    const mutateString = (originalString, options = {}) => {
      const { charValidator = () => true } = options;
      let newString = "";
      for (const char of originalString) {
        if (charValidator(char) && random.choose(settings.MUTATE_UNICODE_ESCAPE_PROB)) {
          newString += toUnicodeEscape(char, options);
        } else {
          newString += char;
        }
      }
      return newString;
    };

    return {
      Identifier(path) {
        if (!shouldMutateIdentifiers) {
          return;
        }
        const node = path.node;
        const name = node.name;

        if (name.startsWith('__') || name.startsWith('%')) {
          return;
        }

        const newName = mutateString(name, { allowX: false });
        if (newName !== name) {
          node.name = newName;
          path.skip();
        }
      },

      StringLiteral(path) {
        if (!shouldMutateStringLiterals) {
          return;
        }
        const node = path.node;
        const originalValue = node.value;

        const newRawContent = mutateString(originalValue);

        if (newRawContent === originalValue) {
            return;
        }

        const quote = node.extra.raw[0];
        const newRawValue = quote + newRawContent + quote;

        node.extra.raw = newRawValue;
        path.skip();
      },

      TemplateLiteral(path) {
        if (!shouldMutateStringLiterals) {
          return;
        }
        const node = path.node;

        for (const quasi of node.quasis) {
          const originalCookedValue = quasi.value.cooked;

          if (originalCookedValue === null) {
            continue;
          }

          const newRawContent = mutateString(originalCookedValue);

          if (newRawContent !== originalCookedValue) {
            quasi.value.raw = newRawContent;
          }
        }
      },

      RegExpLiteral(path) {
        if (!shouldMutateRegExpLiterals) {
          return;
        }
        const node = path.node;
        const pattern = node.pattern;

        const flags = node.flags || '';

        const isSafeRegexChar = (char) => /[a-zA-Z0-9]/.test(char);
        const newPattern = mutateString(pattern, { charValidator: isSafeRegexChar });

        if (newPattern !== pattern) {
          node.pattern = newPattern;
          if (!flags.includes('u')) {
            node.flags = flags + 'u';
          }
          path.skip();
        }
      }
    }
  }
}

module.exports = {
  StringUnicodeMutator: StringUnicodeMutator,
};
