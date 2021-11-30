"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;

var _iterateJsdoc = _interopRequireDefault(require("../iterateJsdoc"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

const middleAsterisks = /^([\t ]|\*(?!\*))+/u;

var _default = (0, _iterateJsdoc.default)(({
  context,
  jsdoc,
  utils
}) => {
  const {
    preventAtEnd = true,
    preventAtMiddleLines = true
  } = context.options[0] || {};
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
      end
    } = tokens;

    if (preventAtMiddleLines && !end && !tag && !type && !name && middleAsterisks.test(description)) {
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
    const endAsterisks = isSingleLineBlock ? /\*((?:\*|(?: |\t))*)\*$/u : /((?:\*|(?: |\t))*)\*$/u;
    const endingAsterisksAndSpaces = (description + delim).match(endAsterisks);

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