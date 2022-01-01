"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;

var _iterateJsdoc = _interopRequireDefault(require("../iterateJsdoc"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

const middleAsterisksBlockWS = /^([\t ]|\*(?!\*))+/u;
const middleAsterisksNoBlockWS = /^\*+/u;
const endAsterisksSingleLineBlockWS = /\*((?:\*|(?: |\t))*)\*$/u;
const endAsterisksMultipleLineBlockWS = /((?:\*|(?: |\t))*)\*$/u;
const endAsterisksSingleLineNoBlockWS = /\*(\**)\*$/u;
const endAsterisksMultipleLineNoBlockWS = /(\**)\*$/u;

var _default = (0, _iterateJsdoc.default)(({
  context,
  jsdoc,
  utils
}) => {
  const {
    allowWhitespace = false,
    preventAtEnd = true,
    preventAtMiddleLines = true
  } = context.options[0] || {};
  const middleAsterisks = allowWhitespace ? middleAsterisksNoBlockWS : middleAsterisksBlockWS; // eslint-disable-next-line complexity -- Todo

  jsdoc.source.some(({
    tokens,
    number
  }) => {
    const {
      delimiter,
      tag,
      name,
      type,
      description,
      end,
      postDelimiter
    } = tokens;

    if (preventAtMiddleLines && !end && !tag && !type && !name && (!allowWhitespace && middleAsterisks.test(description) || allowWhitespace && middleAsterisks.test(postDelimiter + description))) {
      // console.log('description', JSON.stringify(description));
      const fix = () => {
        tokens.description = description.replace(middleAsterisks, '');
      };

      utils.reportJSDoc('Should be no multiple asterisks on middle lines.', {
        line: number
      }, fix, true);
      return true;
    }

    if (!preventAtEnd || !end) {
      return false;
    }

    const isSingleLineBlock = delimiter === '/**';
    const delim = isSingleLineBlock ? '*' : delimiter;
    let endAsterisks;

    if (allowWhitespace) {
      endAsterisks = isSingleLineBlock ? endAsterisksSingleLineNoBlockWS : endAsterisksMultipleLineNoBlockWS;
    } else {
      endAsterisks = isSingleLineBlock ? endAsterisksSingleLineBlockWS : endAsterisksMultipleLineBlockWS;
    }

    const endingAsterisksAndSpaces = (allowWhitespace ? postDelimiter + description + delim : description + delim).match(endAsterisks);

    if (!endingAsterisksAndSpaces || !isSingleLineBlock && endingAsterisksAndSpaces[1] && !endingAsterisksAndSpaces[1].trim()) {
      return false;
    }

    const endFix = () => {
      if (!isSingleLineBlock) {
        tokens.delimiter = '';
      }

      tokens.description = (description + delim).replace(endAsterisks, '');
    };

    utils.reportJSDoc('Should be no multiple asterisks on end lines.', {
      line: number
    }, endFix, true);
    return true;
  });
}, {
  iterateAllJsdocs: true,
  meta: {
    docs: {
      description: '',
      url: 'https://github.com/gajus/eslint-plugin-jsdoc#eslint-plugin-jsdoc-rules-no-multi-asterisks'
    },
    fixable: 'code',
    schema: [{
      additionalProperies: false,
      properties: {
        allowWhitespace: {
          type: 'boolean'
        },
        preventAtEnd: {
          type: 'boolean'
        },
        preventAtMiddleLines: {
          type: 'boolean'
        }
      },
      type: 'object'
    }],
    type: 'suggestion'
  }
});

exports.default = _default;
module.exports = exports.default;
//# sourceMappingURL=noMultiAsterisks.js.map