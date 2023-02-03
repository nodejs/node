"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;
var _iterateJsdoc = _interopRequireDefault(require("../iterateJsdoc"));
function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }
var _default = (0, _iterateJsdoc.default)(({
  context,
  jsdoc,
  utils,
  indent
}) => {
  const [defaultRequireValue = 'always', {
    tags: tagMap = {}
  } = {}] = context.options;
  const {
    source
  } = jsdoc;
  const always = defaultRequireValue === 'always';
  const never = defaultRequireValue === 'never';
  let currentTag;
  source.some(({
    number,
    tokens
  }) => {
    var _tagMap$any2;
    const {
      delimiter,
      tag,
      end,
      description
    } = tokens;
    const neverFix = () => {
      tokens.delimiter = '';
      tokens.postDelimiter = '';
    };
    const checkNever = checkValue => {
      var _tagMap$always, _tagMap$never;
      if (delimiter && delimiter !== '/**' && (never && !((_tagMap$always = tagMap.always) !== null && _tagMap$always !== void 0 && _tagMap$always.includes(checkValue)) || (_tagMap$never = tagMap.never) !== null && _tagMap$never !== void 0 && _tagMap$never.includes(checkValue))) {
        utils.reportJSDoc('Expected JSDoc line to have no prefix.', {
          column: 0,
          line: number
        }, neverFix);
        return true;
      }
      return false;
    };
    const alwaysFix = () => {
      if (!tokens.start) {
        tokens.start = indent + ' ';
      }
      tokens.delimiter = '*';
      tokens.postDelimiter = tag || description ? ' ' : '';
    };
    const checkAlways = checkValue => {
      var _tagMap$never2, _tagMap$always2;
      if (!delimiter && (always && !((_tagMap$never2 = tagMap.never) !== null && _tagMap$never2 !== void 0 && _tagMap$never2.includes(checkValue)) || (_tagMap$always2 = tagMap.always) !== null && _tagMap$always2 !== void 0 && _tagMap$always2.includes(checkValue))) {
        utils.reportJSDoc('Expected JSDoc line to have the prefix.', {
          column: 0,
          line: number
        }, alwaysFix);
        return true;
      }
      return false;
    };
    if (tag) {
      // Remove at sign
      currentTag = tag.slice(1);
    }
    if (
    // If this is the end but has a tag, the delimiter will also be
    //  populated and will be safely ignored later
    end && !tag) {
      return false;
    }
    if (!currentTag) {
      var _tagMap$any;
      if ((_tagMap$any = tagMap.any) !== null && _tagMap$any !== void 0 && _tagMap$any.includes('*description')) {
        return false;
      }
      if (checkNever('*description')) {
        return true;
      }
      if (checkAlways('*description')) {
        return true;
      }
      return false;
    }
    if ((_tagMap$any2 = tagMap.any) !== null && _tagMap$any2 !== void 0 && _tagMap$any2.includes(currentTag)) {
      return false;
    }
    if (checkNever(currentTag)) {
      return true;
    }
    if (checkAlways(currentTag)) {
      return true;
    }
    return false;
  });
}, {
  iterateAllJsdocs: true,
  meta: {
    fixable: 'code',
    schema: [{
      enum: ['always', 'never', 'any'],
      type: 'string'
    }, {
      additionalProperties: false,
      properties: {
        tags: {
          properties: {
            always: {
              items: {
                type: 'string'
              },
              type: 'array'
            },
            any: {
              items: {
                type: 'string'
              },
              type: 'array'
            },
            never: {
              items: {
                type: 'string'
              },
              type: 'array'
            }
          },
          type: 'object'
        }
      },
      type: 'object'
    }],
    type: 'layout'
  }
});
exports.default = _default;
module.exports = exports.default;
//# sourceMappingURL=requireAsteriskPrefix.js.map