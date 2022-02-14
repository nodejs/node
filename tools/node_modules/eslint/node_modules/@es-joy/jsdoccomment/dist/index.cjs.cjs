'use strict';

Object.defineProperty(exports, '__esModule', { value: true });

var jsdocTypePrattParser = require('jsdoc-type-pratt-parser');
var esquery = require('esquery');
var commentParser = require('comment-parser');

function _interopDefaultLegacy (e) { return e && typeof e === 'object' && 'default' in e ? e : { 'default': e }; }

var esquery__default = /*#__PURE__*/_interopDefaultLegacy(esquery);

/**
 * Removes initial and ending brackets from `rawType`
 * @param {JsdocTypeLine[]|JsdocTag} container
 * @param {boolean} isArr
 * @returns {void}
 */

const stripEncapsulatingBrackets = (container, isArr) => {
  if (isArr) {
    const firstItem = container[0];
    firstItem.rawType = firstItem.rawType.replace(/^\{/u, '');
    const lastItem = container[container.length - 1];
    lastItem.rawType = lastItem.rawType.replace(/\}$/u, '');
    return;
  }

  container.rawType = container.rawType.replace(/^\{/u, '').replace(/\}$/u, '');
};
/**
 * Strips brackets from a tag's `rawType` values and adds `parsedType`
 * @param {JsdocTag} lastTag
 * @param {external:JsdocTypePrattParserMode} mode
 * @returns {void}
 */


const cleanUpLastTag = (lastTag, mode) => {
  // Strip out `}` that encapsulates and is not part of
  //   the type
  stripEncapsulatingBrackets(lastTag);

  if (lastTag.typeLines.length) {
    stripEncapsulatingBrackets(lastTag.typeLines, true);
  } // With even a multiline type now in full, add parsing


  let parsedType = null;

  try {
    parsedType = jsdocTypePrattParser.parse(lastTag.rawType, mode);
  } catch {// Ignore
  }

  lastTag.parsedType = parsedType;
};
/**
 * @external CommentParserJsdoc
 */

/**
 * @external JsdocTypePrattParserMode
 */

/**
 * @typedef {{
 *   delimiter: string,
 *   postDelimiter: string,
 *   rawType: string,
 *   start: string,
 *   type: "JsdocTypeLine"
 * }} JsdocTypeLine
 */

/**
 * @typedef {{
 *   delimiter: string,
 *   description: string,
 *   postDelimiter: string,
 *   start: string,
 *   type: "JsdocDescriptionLine"
 * }} JsdocDescriptionLine
 */

/**
 * @typedef {{
 *   delimiter: string,
 *   description: string,
 *   postDelimiter: string,
 *   start: string,
 *   tag: string,
 *   end: string,
 *   type: string,
 *   descriptionLines: JsdocDescriptionLine[],
 *   rawType: string,
 *   type: "JsdocTag",
 *   typeLines: JsdocTypeLine[]
 * }} JsdocTag
 */

/**
 * @typedef {{
 *   delimiter: string,
 *   description: string,
 *   descriptionLines: JsdocDescriptionLine[],
 *   end: string,
 *   postDelimiter: string,
 *   lineEnd: string,
 *   type: "JsdocBlock",
 *   lastDescriptionLine: Integer,
 *   tags: JsdocTag[]
 * }} JsdocBlock
 */

/**
 *
 * @param {external:CommentParserJsdoc} jsdoc
 * @param {external:JsdocTypePrattParserMode} mode
 * @returns {JsdocBlock}
 */


const commentParserToESTree = (jsdoc, mode) => {
  const {
    source
  } = jsdoc;
  const {
    tokens: {
      delimiter: delimiterRoot,
      lineEnd: lineEndRoot,
      postDelimiter: postDelimiterRoot,
      end: endRoot,
      description: descriptionRoot
    }
  } = source[0];
  const ast = {
    delimiter: delimiterRoot,
    description: descriptionRoot,
    descriptionLines: [],
    // `end` will be overwritten if there are other entries
    end: endRoot,
    endLine: source.length - 1,
    postDelimiter: postDelimiterRoot,
    lineEnd: lineEndRoot,
    type: 'JsdocBlock'
  };
  const tags = [];
  let lastDescriptionLine;
  let lastTag = null;
  source.forEach((info, idx) => {
    const {
      tokens
    } = info;
    const {
      delimiter,
      description,
      postDelimiter,
      start,
      tag,
      end,
      type: rawType
    } = tokens;

    if (tag || end) {
      if (lastDescriptionLine === undefined) {
        lastDescriptionLine = idx;
      } // Clean-up with last tag before end or new tag


      if (lastTag) {
        cleanUpLastTag(lastTag, mode);
      } // Stop the iteration when we reach the end
      // but only when there is no tag earlier in the line
      // to still process


      if (end && !tag) {
        ast.end = end;
        return;
      }

      const {
        end: ed,
        ...tkns
      } = tokens;
      const tagObj = { ...tkns,
        descriptionLines: [],
        rawType: '',
        type: 'JsdocTag',
        typeLines: []
      };
      tagObj.tag = tagObj.tag.replace(/^@/u, '');
      lastTag = tagObj;
      tags.push(tagObj);
    }

    if (rawType) {
      // Will strip rawType brackets after this tag
      lastTag.typeLines.push({
        delimiter,
        postDelimiter,
        rawType,
        start,
        type: 'JsdocTypeLine'
      });
      lastTag.rawType += rawType;
    }

    if (description) {
      const holder = lastTag || ast;
      holder.descriptionLines.push({
        delimiter,
        description,
        postDelimiter,
        start,
        type: 'JsdocDescriptionLine'
      });
      holder.description += holder.description ? '\n' + description : description;
    } // Clean-up where last line itself has tag content


    if (end && tag) {
      ast.end = end;
      cleanUpLastTag(lastTag, mode);
    }
  });
  ast.lastDescriptionLine = lastDescriptionLine;
  ast.tags = tags;
  return ast;
};

const jsdocVisitorKeys = {
  JsdocBlock: ['tags', 'descriptionLines'],
  JsdocDescriptionLine: [],
  JsdocTypeLine: [],
  JsdocTag: ['descriptionLines', 'typeLines', 'parsedType']
};

/**
 * @callback CommentHandler
 * @param {string} commentSelector
 * @param {Node} jsdoc
 * @returns {boolean}
 */

/**
 * @param {Settings} settings
 * @returns {CommentHandler}
 */

const commentHandler = settings => {
  /**
   * @type {CommentHandler}
   */
  return (commentSelector, jsdoc) => {
    const {
      mode
    } = settings;
    const selector = esquery__default["default"].parse(commentSelector);
    const ast = commentParserToESTree(jsdoc, mode);
    return esquery__default["default"].matches(ast, selector, null, {
      visitorKeys: { ...jsdocTypePrattParser.visitorKeys,
        ...jsdocVisitorKeys
      }
    });
  };
};

const toCamelCase = str => {
  return str.toLowerCase().replace(/^[a-z]/gu, init => {
    return init.toUpperCase();
  }).replace(/_(?<wordInit>[a-z])/gu, (_, n1, o, s, {
    wordInit
  }) => {
    return wordInit.toUpperCase();
  });
};

/* eslint-disable prefer-named-capture-group -- Temporary */
const {
  seedBlock,
  seedTokens
} = commentParser.util;
const {
  name: nameTokenizer,
  tag: tagTokenizer,
  type: typeTokenizer,
  description: descriptionTokenizer
} = commentParser.tokenizers;
const hasSeeWithLink = spec => {
  return spec.tag === 'see' && /\{@link.+?\}/u.test(spec.source[0].source);
};
const defaultNoTypes = ['default', 'defaultvalue', 'see'];
const defaultNoNames = ['access', 'author', 'default', 'defaultvalue', 'description', 'example', 'exception', 'kind', 'license', 'return', 'returns', 'since', 'summary', 'throws', 'version', 'variation'];

const getTokenizers = ({
  noTypes = defaultNoTypes,
  noNames = defaultNoNames
} = {}) => {
  // trim
  return [// Tag
  tagTokenizer(), // Type
  spec => {
    if (noTypes.includes(spec.tag)) {
      return spec;
    }

    return typeTokenizer()(spec);
  }, // Name
  spec => {
    if (spec.tag === 'template') {
      // const preWS = spec.postTag;
      const remainder = spec.source[0].tokens.description;
      const pos = remainder.search(/(?<![\s,])\s/u);
      const name = pos === -1 ? remainder : remainder.slice(0, pos);
      const extra = remainder.slice(pos);
      let postName = '',
          description = '',
          lineEnd = '';

      if (pos > -1) {
        [, postName, description, lineEnd] = extra.match(/(\s*)([^\r]*)(\r)?/u);
      }

      spec.name = name;
      spec.optional = false;
      const {
        tokens
      } = spec.source[0];
      tokens.name = name;
      tokens.postName = postName;
      tokens.description = description;
      tokens.lineEnd = lineEnd || '';
      return spec;
    }

    if (noNames.includes(spec.tag) || hasSeeWithLink(spec)) {
      return spec;
    }

    return nameTokenizer()(spec);
  }, // Description
  spec => {
    return descriptionTokenizer('preserve')(spec);
  }];
};
/**
 *
 * @param {PlainObject} commentNode
 * @param {string} [indent=""] Whitespace
 * @returns {PlainObject}
 */


const parseComment = (commentNode, indent = '') => {
  // Preserve JSDoc block start/end indentation.
  return commentParser.parse(`/*${commentNode.value}*/`, {
    // @see https://github.com/yavorskiy/comment-parser/issues/21
    tokenizers: getTokenizers()
  })[0] || seedBlock({
    source: [{
      number: 0,
      tokens: seedTokens({
        delimiter: '/**'
      })
    }, {
      number: 1,
      tokens: seedTokens({
        end: '*/',
        start: indent + ' '
      })
    }]
  });
};

/**
 * Obtained originally from {@link https://github.com/eslint/eslint/blob/master/lib/util/source-code.js#L313}.
 *
 * @license MIT
 */

/**
 * Checks if the given token is a comment token or not.
 *
 * @param {Token} token - The token to check.
 * @returns {boolean} `true` if the token is a comment token.
 */
const isCommentToken = token => {
  return token.type === 'Line' || token.type === 'Block' || token.type === 'Shebang';
};
/**
 * @param {AST} node
 * @returns {boolean}
 */


const getDecorator = node => {
  var _node$declaration, _node$declaration$dec, _node$decorators, _node$parent, _node$parent$decorato;

  return (node === null || node === void 0 ? void 0 : (_node$declaration = node.declaration) === null || _node$declaration === void 0 ? void 0 : (_node$declaration$dec = _node$declaration.decorators) === null || _node$declaration$dec === void 0 ? void 0 : _node$declaration$dec[0]) || (node === null || node === void 0 ? void 0 : (_node$decorators = node.decorators) === null || _node$decorators === void 0 ? void 0 : _node$decorators[0]) || (node === null || node === void 0 ? void 0 : (_node$parent = node.parent) === null || _node$parent === void 0 ? void 0 : (_node$parent$decorato = _node$parent.decorators) === null || _node$parent$decorato === void 0 ? void 0 : _node$parent$decorato[0]);
};
/**
 * Check to see if it is a ES6 export declaration.
 *
 * @param {ASTNode} astNode An AST node.
 * @returns {boolean} whether the given node represents an export declaration.
 * @private
 */


const looksLikeExport = function (astNode) {
  return astNode.type === 'ExportDefaultDeclaration' || astNode.type === 'ExportNamedDeclaration' || astNode.type === 'ExportAllDeclaration' || astNode.type === 'ExportSpecifier';
};
/**
 * @param {AST} astNode
 * @returns {AST}
 */


const getTSFunctionComment = function (astNode) {
  const {
    parent
  } = astNode;
  const grandparent = parent.parent;
  const greatGrandparent = grandparent.parent;
  const greatGreatGrandparent = greatGrandparent && greatGrandparent.parent; // istanbul ignore if

  if (parent.type !== 'TSTypeAnnotation') {
    return astNode;
  }

  switch (grandparent.type) {
    case 'PropertyDefinition':
    case 'ClassProperty':
    case 'TSDeclareFunction':
    case 'TSMethodSignature':
    case 'TSPropertySignature':
      return grandparent;

    case 'ArrowFunctionExpression':
      // istanbul ignore else
      if (greatGrandparent.type === 'VariableDeclarator' // && greatGreatGrandparent.parent.type === 'VariableDeclaration'
      ) {
        return greatGreatGrandparent.parent;
      } // istanbul ignore next


      return astNode;

    case 'FunctionExpression':
      // istanbul ignore else
      if (greatGrandparent.type === 'MethodDefinition') {
        return greatGrandparent;
      }

    // Fallthrough

    default:
      // istanbul ignore if
      if (grandparent.type !== 'Identifier') {
        // istanbul ignore next
        return astNode;
      }

  } // istanbul ignore next


  switch (greatGrandparent.type) {
    case 'ArrowFunctionExpression':
      // istanbul ignore else
      if (greatGreatGrandparent.type === 'VariableDeclarator' && greatGreatGrandparent.parent.type === 'VariableDeclaration') {
        return greatGreatGrandparent.parent;
      } // istanbul ignore next


      return astNode;

    case 'FunctionDeclaration':
      return greatGrandparent;

    case 'VariableDeclarator':
      // istanbul ignore else
      if (greatGreatGrandparent.type === 'VariableDeclaration') {
        return greatGreatGrandparent;
      }

    // Fallthrough

    default:
      // istanbul ignore next
      return astNode;
  }
};

const invokedExpression = new Set(['CallExpression', 'OptionalCallExpression', 'NewExpression']);
const allowableCommentNode = new Set(['VariableDeclaration', 'ExpressionStatement', 'MethodDefinition', 'Property', 'ObjectProperty', 'ClassProperty', 'PropertyDefinition', 'ExportDefaultDeclaration']);
/**
 * Reduces the provided node to the appropriate node for evaluating
 * JSDoc comment status.
 *
 * @param {ASTNode} node An AST node.
 * @param {SourceCode} sourceCode The ESLint SourceCode.
 * @returns {ASTNode} The AST node that can be evaluated for appropriate
 * JSDoc comments.
 */

const getReducedASTNode = function (node, sourceCode) {
  let {
    parent
  } = node;

  switch (node.type) {
    case 'TSFunctionType':
      return getTSFunctionComment(node);

    case 'TSInterfaceDeclaration':
    case 'TSTypeAliasDeclaration':
    case 'TSEnumDeclaration':
    case 'ClassDeclaration':
    case 'FunctionDeclaration':
      return looksLikeExport(parent) ? parent : node;

    case 'TSDeclareFunction':
    case 'ClassExpression':
    case 'ObjectExpression':
    case 'ArrowFunctionExpression':
    case 'TSEmptyBodyFunctionExpression':
    case 'FunctionExpression':
      if (!invokedExpression.has(parent.type)) {
        while (!sourceCode.getCommentsBefore(parent).length && !/Function/u.test(parent.type) && !allowableCommentNode.has(parent.type)) {
          ({
            parent
          } = parent);

          if (!parent) {
            break;
          }
        }

        if (parent && parent.type !== 'FunctionDeclaration' && parent.type !== 'Program') {
          if (parent.parent && parent.parent.type === 'ExportNamedDeclaration') {
            return parent.parent;
          }

          return parent;
        }
      }

      return node;

    default:
      return node;
  }
};
/**
 * Checks for the presence of a JSDoc comment for the given node and returns it.
 *
 * @param {ASTNode} astNode The AST node to get the comment for.
 * @param {SourceCode} sourceCode
 * @param {{maxLines: Integer, minLines: Integer}} settings
 * @returns {Token|null} The Block comment token containing the JSDoc comment
 *    for the given node or null if not found.
 * @private
 */


const findJSDocComment = (astNode, sourceCode, settings) => {
  const {
    minLines,
    maxLines
  } = settings;
  let currentNode = astNode;
  let tokenBefore = null;

  while (currentNode) {
    const decorator = getDecorator(currentNode);

    if (decorator) {
      currentNode = decorator;
    }

    tokenBefore = sourceCode.getTokenBefore(currentNode, {
      includeComments: true
    });

    if (tokenBefore && tokenBefore.type === 'Punctuator' && tokenBefore.value === '(') {
      [tokenBefore] = sourceCode.getTokensBefore(currentNode, {
        count: 2,
        includeComments: true
      });
    }

    if (!tokenBefore || !isCommentToken(tokenBefore)) {
      return null;
    }

    if (tokenBefore.type === 'Line') {
      currentNode = tokenBefore;
      continue;
    }

    break;
  }

  if (tokenBefore.type === 'Block' && tokenBefore.value.charAt(0) === '*' && currentNode.loc.start.line - tokenBefore.loc.end.line >= minLines && currentNode.loc.start.line - tokenBefore.loc.end.line <= maxLines) {
    return tokenBefore;
  }

  return null;
};
/**
 * Retrieves the JSDoc comment for a given node.
 *
 * @param {SourceCode} sourceCode The ESLint SourceCode
 * @param {ASTNode} node The AST node to get the comment for.
 * @param {PlainObject} settings The settings in context
 * @returns {Token|null} The Block comment token containing the JSDoc comment
 *    for the given node or null if not found.
 * @public
 */


const getJSDocComment = function (sourceCode, node, settings) {
  const reducedNode = getReducedASTNode(node, sourceCode);
  return findJSDocComment(reducedNode, sourceCode, settings);
};

Object.defineProperty(exports, 'jsdocTypeVisitorKeys', {
  enumerable: true,
  get: function () { return jsdocTypePrattParser.visitorKeys; }
});
exports.commentHandler = commentHandler;
exports.commentParserToESTree = commentParserToESTree;
exports.defaultNoNames = defaultNoNames;
exports.defaultNoTypes = defaultNoTypes;
exports.findJSDocComment = findJSDocComment;
exports.getDecorator = getDecorator;
exports.getJSDocComment = getJSDocComment;
exports.getReducedASTNode = getReducedASTNode;
exports.getTokenizers = getTokenizers;
exports.hasSeeWithLink = hasSeeWithLink;
exports.jsdocVisitorKeys = jsdocVisitorKeys;
exports.parseComment = parseComment;
exports.toCamelCase = toCamelCase;
Object.keys(jsdocTypePrattParser).forEach(function (k) {
  if (k !== 'default' && !exports.hasOwnProperty(k)) Object.defineProperty(exports, k, {
    enumerable: true,
    get: function () { return jsdocTypePrattParser[k]; }
  });
});
