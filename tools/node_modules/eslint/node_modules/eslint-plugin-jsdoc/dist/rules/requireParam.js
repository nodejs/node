"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;
var _iterateJsdoc = _interopRequireDefault(require("../iterateJsdoc"));
function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }
/**
 * @template T
 * @param {string[]} desiredRoots
 * @param {number} currentIndex
 * @returns {[string, boolean, () => T]}
 */
const rootNamer = (desiredRoots, currentIndex) => {
  let name;
  let idx = currentIndex;
  const incremented = desiredRoots.length <= 1;
  if (incremented) {
    const base = desiredRoots[0];
    const suffix = idx++;
    name = `${base}${suffix}`;
  } else {
    name = desiredRoots.shift();
  }
  return [name, incremented, () => {
    return rootNamer(desiredRoots, idx);
  }];
};

// eslint-disable-next-line complexity
var _default = (0, _iterateJsdoc.default)(({
  jsdoc,
  utils,
  context
}) => {
  if (utils.avoidDocs()) {
    return;
  }

  // Param type is specified by type in @type
  if (utils.hasTag('type')) {
    return;
  }
  const {
    autoIncrementBase = 0,
    checkRestProperty = false,
    checkDestructured = true,
    checkDestructuredRoots = true,
    checkTypesPattern = '/^(?:[oO]bject|[aA]rray|PlainObject|Generic(?:Object|Array))$/',
    enableFixer = true,
    enableRootFixer = true,
    enableRestElementFixer = true,
    unnamedRootBase = ['root'],
    useDefaultObjectProperties = false
  } = context.options[0] || {};
  const preferredTagName = utils.getPreferredTagName({
    tagName: 'param'
  });
  if (!preferredTagName) {
    return;
  }
  const functionParameterNames = utils.getFunctionParameterNames(useDefaultObjectProperties);
  if (!functionParameterNames.length) {
    return;
  }
  const jsdocParameterNames = utils.getJsdocTagsDeep(preferredTagName);
  const shallowJsdocParameterNames = jsdocParameterNames.filter(tag => {
    return !tag.name.includes('.');
  }).map((tag, idx) => {
    return {
      ...tag,
      idx
    };
  });
  const checkTypesRegex = utils.getRegexFromString(checkTypesPattern);
  const missingTags = [];
  const flattenedRoots = utils.flattenRoots(functionParameterNames).names;
  const paramIndex = {};
  const hasParamIndex = cur => {
    return utils.dropPathSegmentQuotes(String(cur)) in paramIndex;
  };
  const getParamIndex = cur => {
    return paramIndex[utils.dropPathSegmentQuotes(String(cur))];
  };
  const setParamIndex = (cur, idx) => {
    paramIndex[utils.dropPathSegmentQuotes(String(cur))] = idx;
  };
  for (const [idx, cur] of flattenedRoots.entries()) {
    setParamIndex(cur, idx);
  }
  const findExpectedIndex = (jsdocTags, indexAtFunctionParams) => {
    const remainingRoots = functionParameterNames.slice(indexAtFunctionParams || 0);
    const foundIndex = jsdocTags.findIndex(({
      name,
      newAdd
    }) => {
      return !newAdd && remainingRoots.some(remainingRoot => {
        if (Array.isArray(remainingRoot)) {
          return remainingRoot[1].names.includes(name);
        }
        if (typeof remainingRoot === 'object') {
          return name === remainingRoot.name;
        }
        return name === remainingRoot;
      });
    });
    const tags = foundIndex > -1 ? jsdocTags.slice(0, foundIndex) : jsdocTags.filter(({
      tag
    }) => {
      return tag === preferredTagName;
    });
    let tagLineCount = 0;
    for (const {
      source
    } of tags) {
      for (const {
        tokens: {
          end
        }
      } of source) {
        if (!end) {
          tagLineCount++;
        }
      }
    }
    return tagLineCount;
  };
  let [nextRootName, incremented, namer] = rootNamer([...unnamedRootBase], autoIncrementBase);
  for (const [functionParameterIdx, functionParameterName] of functionParameterNames.entries()) {
    let inc;
    if (Array.isArray(functionParameterName)) {
      const matchedJsdoc = shallowJsdocParameterNames[functionParameterIdx] || jsdocParameterNames[functionParameterIdx];
      let rootName;
      if (functionParameterName[0]) {
        rootName = functionParameterName[0];
      } else if (matchedJsdoc && matchedJsdoc.name) {
        rootName = matchedJsdoc.name;
        if (matchedJsdoc.type && matchedJsdoc.type.search(checkTypesRegex) === -1) {
          continue;
        }
      } else {
        rootName = nextRootName;
        inc = incremented;
        [nextRootName, incremented, namer] = namer();
      }
      const {
        hasRestElement,
        hasPropertyRest,
        rests,
        names
      } = functionParameterName[1];
      const notCheckingNames = [];
      if (!enableRestElementFixer && hasRestElement) {
        continue;
      }
      if (!checkDestructuredRoots) {
        continue;
      }
      for (const [idx, paramName] of names.entries()) {
        // Add root if the root name is not in the docs (and is not already
        //  in the tags to be fixed)
        if (!jsdocParameterNames.find(({
          name
        }) => {
          return name === rootName;
        }) && !missingTags.find(({
          functionParameterName: fpn
        }) => {
          return fpn === rootName;
        })) {
          const emptyParamIdx = jsdocParameterNames.findIndex(({
            name
          }) => {
            return !name;
          });
          if (emptyParamIdx > -1) {
            missingTags.push({
              functionParameterIdx: emptyParamIdx,
              functionParameterName: rootName,
              inc,
              remove: true
            });
          } else {
            missingTags.push({
              functionParameterIdx: hasParamIndex(rootName) ? getParamIndex(rootName) : getParamIndex(paramName),
              functionParameterName: rootName,
              inc
            });
          }
        }
        if (!checkDestructured) {
          continue;
        }
        if (!checkRestProperty && rests[idx]) {
          continue;
        }
        const fullParamName = `${rootName}.${paramName}`;
        const notCheckingName = jsdocParameterNames.find(({
          name,
          type: paramType
        }) => {
          return utils.comparePaths(name)(fullParamName) && paramType.search(checkTypesRegex) === -1 && paramType !== '';
        });
        if (notCheckingName !== undefined) {
          notCheckingNames.push(notCheckingName.name);
        }
        if (notCheckingNames.find(name => {
          return fullParamName.startsWith(name);
        })) {
          continue;
        }
        if (jsdocParameterNames && !jsdocParameterNames.find(({
          name
        }) => {
          return utils.comparePaths(name)(fullParamName);
        })) {
          missingTags.push({
            functionParameterIdx: getParamIndex(functionParameterName[0] ? fullParamName : paramName),
            functionParameterName: fullParamName,
            inc,
            type: hasRestElement && !hasPropertyRest ? '{...any}' : undefined
          });
        }
      }
      continue;
    }
    let funcParamName;
    let type;
    if (typeof functionParameterName === 'object') {
      if (!enableRestElementFixer && functionParameterName.restElement) {
        continue;
      }
      funcParamName = functionParameterName.name;
      type = '{...any}';
    } else {
      funcParamName = functionParameterName;
    }
    if (jsdocParameterNames && !jsdocParameterNames.find(({
      name
    }) => {
      return name === funcParamName;
    }) && funcParamName !== 'this') {
      missingTags.push({
        functionParameterIdx: getParamIndex(funcParamName),
        functionParameterName: funcParamName,
        inc,
        type
      });
    }
  }
  const fix = ({
    functionParameterIdx,
    functionParameterName,
    remove,
    inc,
    type
  }) => {
    if (inc && !enableRootFixer) {
      return;
    }
    const createTokens = (tagIndex, sourceIndex, spliceCount) => {
      // console.log(sourceIndex, tagIndex, jsdoc.tags, jsdoc.source);
      const tokens = {
        number: sourceIndex + 1,
        tokens: {
          delimiter: '*',
          description: '',
          end: '',
          lineEnd: '',
          name: functionParameterName,
          newAdd: true,
          postDelimiter: ' ',
          postName: '',
          postTag: ' ',
          postType: type ? ' ' : '',
          start: jsdoc.source[sourceIndex].tokens.start,
          tag: `@${preferredTagName}`,
          type: type ?? ''
        }
      };
      jsdoc.tags.splice(tagIndex, spliceCount, {
        name: functionParameterName,
        newAdd: true,
        source: [tokens],
        tag: preferredTagName,
        type: type ?? ''
      });
      const firstNumber = jsdoc.source[0].number;
      jsdoc.source.splice(sourceIndex, spliceCount, tokens);
      for (const [idx, src] of jsdoc.source.slice(sourceIndex).entries()) {
        src.number = firstNumber + sourceIndex + idx;
      }
    };
    const offset = jsdoc.source.findIndex(({
      tokens: {
        tag,
        end
      }
    }) => {
      return tag || end;
    });
    if (remove) {
      createTokens(functionParameterIdx, offset + functionParameterIdx, 1);
    } else {
      const expectedIdx = findExpectedIndex(jsdoc.tags, functionParameterIdx);
      createTokens(expectedIdx, offset + expectedIdx, 0);
    }
  };
  const fixer = () => {
    for (const missingTag of missingTags) {
      fix(missingTag);
    }
  };
  if (missingTags.length && jsdoc.source.length === 1) {
    utils.makeMultiline();
  }
  for (const {
    functionParameterName
  } of missingTags) {
    utils.reportJSDoc(`Missing JSDoc @${preferredTagName} "${functionParameterName}" declaration.`, null, enableFixer ? fixer : null);
  }
}, {
  contextDefaults: true,
  meta: {
    docs: {
      description: 'Requires that all function parameters are documented.',
      url: 'https://github.com/gajus/eslint-plugin-jsdoc#eslint-plugin-jsdoc-rules-require-param'
    },
    fixable: 'code',
    schema: [{
      additionalProperties: false,
      properties: {
        autoIncrementBase: {
          default: 0,
          type: 'integer'
        },
        checkConstructors: {
          default: true,
          type: 'boolean'
        },
        checkDestructured: {
          default: true,
          type: 'boolean'
        },
        checkDestructuredRoots: {
          default: true,
          type: 'boolean'
        },
        checkGetters: {
          default: false,
          type: 'boolean'
        },
        checkRestProperty: {
          default: false,
          type: 'boolean'
        },
        checkSetters: {
          default: false,
          type: 'boolean'
        },
        checkTypesPattern: {
          type: 'string'
        },
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
        enableFixer: {
          type: 'boolean'
        },
        enableRestElementFixer: {
          type: 'boolean'
        },
        enableRootFixer: {
          type: 'boolean'
        },
        exemptedBy: {
          items: {
            type: 'string'
          },
          type: 'array'
        },
        unnamedRootBase: {
          items: {
            type: 'string'
          },
          type: 'array'
        },
        useDefaultObjectProperties: {
          type: 'boolean'
        }
      },
      type: 'object'
    }],
    type: 'suggestion'
  },
  // We cannot cache comment nodes as the contexts may recur with the
  //  same comment node but a different JS node, and we may need the different
  //  JS node to ensure we iterate its context
  noTracking: true
});
exports.default = _default;
module.exports = exports.default;
//# sourceMappingURL=requireParam.js.map