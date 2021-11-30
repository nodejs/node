"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;

var _iterateJsdoc = _interopRequireDefault(require("../iterateJsdoc"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

// If supporting Node >= 10, we could loosen the default to this for the
//   initial letter: \\p{Upper}
const matchDescriptionDefault = '^[A-Z`\\d_][\\s\\S]*[.?!`]$';

const stringOrDefault = (value, userDefault) => {
  return typeof value === 'string' ? value : userDefault || matchDescriptionDefault;
};

var _default = (0, _iterateJsdoc.default)(({
  jsdoc,
  report,
  context,
  utils
}) => {
  const {
    mainDescription,
    matchDescription,
    message,
    tags
  } = context.options[0] || {};

  const validateDescription = (description, tag) => {
    let mainDescriptionMatch = mainDescription;
    let errorMessage = message;

    if (typeof mainDescription === 'object') {
      mainDescriptionMatch = mainDescription.match;
      errorMessage = mainDescription.message;
    }

    if (!tag && mainDescriptionMatch === false) {
      return;
    }

    let tagValue = mainDescriptionMatch;

    if (tag) {
      const tagName = tag.tag;

      if (typeof tags[tagName] === 'object') {
        tagValue = tags[tagName].match;
        errorMessage = tags[tagName].message;
      } else {
        tagValue = tags[tagName];
      }
    }

    const regex = utils.getRegexFromString(stringOrDefault(tagValue, matchDescription));

    if (!regex.test(description)) {
      report(errorMessage || 'JSDoc description does not satisfy the regex pattern.', null, tag || {
        // Add one as description would typically be into block
        line: jsdoc.source[0].number + 1
      });
    }
  };

  if (jsdoc.description) {
    const {
      description
    } = utils.getDescription();
    validateDescription(description.replace(/\s+$/u, ''));
  }

  if (!tags || !Object.keys(tags).length) {
    return;
  }

  const hasOptionTag = tagName => {
    return Boolean(tags[tagName]);
  };

  utils.forEachPreferredTag('description', (matchingJsdocTag, targetTagName) => {
    const description = (matchingJsdocTag.name + ' ' + utils.getTagDescription(matchingJsdocTag)).trim();

    if (hasOptionTag(targetTagName)) {
      validateDescription(description, matchingJsdocTag);
    }
  }, true);
  const whitelistedTags = utils.filterTags(({
    tag: tagName
  }) => {
    return hasOptionTag(tagName);
  });
  const {
    tagsWithNames,
    tagsWithoutNames
  } = utils.getTagsByType(whitelistedTags);
  tagsWithNames.some(tag => {
    const description = utils.getTagDescription(tag).replace(/^[- ]*/u, '').trim();
    return validateDescription(description, tag);
  });
  tagsWithoutNames.some(tag => {
    const description = (tag.name + ' ' + utils.getTagDescription(tag)).trim();
    return validateDescription(description, tag);
  });
}, {
  contextDefaults: true,
  meta: {
    docs: {
      description: 'Enforces a regular expression pattern on descriptions.',
      url: 'https://github.com/gajus/eslint-plugin-jsdoc#eslint-plugin-jsdoc-rules-match-description'
    },
    schema: [{
      additionalProperties: false,
      properties: {
        contexts: {
          items: {
            anyOf: [{
              type: 'string'
            }, {
              additionalProperties: false,
              properties: {
                comment: {
                  type: 'string'
                },
                context: {
                  type: 'string'
                }
              },
              type: 'object'
            }]
          },
          type: 'array'
        },
        mainDescription: {
          oneOf: [{
            format: 'regex',
            type: 'string'
          }, {
            type: 'boolean'
          }, {
            additionalProperties: false,
            properties: {
              match: {
                oneOf: [{
                  format: 'regex',
                  type: 'string'
                }, {
                  type: 'boolean'
                }]
              },
              message: {
                type: 'string'
              }
            },
            type: 'object'
          }]
        },
        matchDescription: {
          format: 'regex',
          type: 'string'
        },
        message: {
          type: 'string'
        },
        tags: {
          patternProperties: {
            '.*': {
              oneOf: [{
                format: 'regex',
                type: 'string'
              }, {
                enum: [true],
                type: 'boolean'
              }, {
                additionalProperties: false,
                properties: {
                  match: {
                    oneOf: [{
                      format: 'regex',
                      type: 'string'
                    }, {
                      enum: [true],
                      type: 'boolean'
                    }]
                  },
                  message: {
                    type: 'string'
                  }
                },
                type: 'object'
              }]
            }
          },
          type: 'object'
        }
      },
      type: 'object'
    }],
    type: 'suggestion'
  }
});

exports.default = _default;
module.exports = exports.default;
//# sourceMappingURL=matchDescription.js.map