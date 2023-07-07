'use strict';

var jsdocTypePrattParser = require('jsdoc-type-pratt-parser');
var esquery = require('esquery');
var commentParser = require('comment-parser');

/**
 * Removes initial and ending brackets from `rawType`
 * @param {JsdocTypeLine[]|JsdocTag} container
 * @param {boolean} [isArr]
 * @returns {void}
 */
const stripEncapsulatingBrackets = (container, isArr) => {
  if (isArr) {
    const firstItem = /** @type {JsdocTypeLine[]} */container[0];
    firstItem.rawType = firstItem.rawType.replace(/^\{/u, '');
    const lastItem = /** @type {JsdocTypeLine} */
    /** @type {JsdocTypeLine[]} */container.at(-1);
    lastItem.rawType = lastItem.rawType.replace(/\}$/u, '');
    return;
  }
  /** @type {JsdocTag} */
  container.rawType = /** @type {JsdocTag} */container.rawType.replace(/^\{/u, '').replace(/\}$/u, '');
};

/**
 * @typedef {{
 *   delimiter: string,
 *   postDelimiter: string,
 *   rawType: string,
 *   initial: string,
 *   type: "JsdocTypeLine"
 * }} JsdocTypeLine
 */

/**
 * @typedef {{
 *   delimiter: string,
 *   description: string,
 *   postDelimiter: string,
 *   initial: string,
 *   type: "JsdocDescriptionLine"
 * }} JsdocDescriptionLine
 */

/**
 * @typedef {{
 *   format: 'pipe' | 'plain' | 'prefix' | 'space',
 *   namepathOrURL: string,
 *   tag: string,
 *   text: string,
 * }} JsdocInlineTagNoType
 */
/**
 * @typedef {JsdocInlineTagNoType & {
 *   type: "JsdocInlineTag"
 * }} JsdocInlineTag
 */

/**
 * @typedef {{
 *   delimiter: string,
 *   description: string,
 *   descriptionLines: JsdocDescriptionLine[],
 *   initial: string,
 *   inlineTags: JsdocInlineTag[]
 *   name: string,
 *   postDelimiter: string,
 *   postName: string,
 *   postTag: string,
 *   postType: string,
 *   rawType: string,
 *   parsedType: import('jsdoc-type-pratt-parser').RootResult|null
 *   tag: string,
 *   type: "JsdocTag",
 *   typeLines: JsdocTypeLine[],
 * }} JsdocTag
 */

/**
 * @typedef {number} Integer
 */

/**
 * @typedef {{
 *   delimiter: string,
 *   description: string,
 *   descriptionEndLine?: Integer,
 *   descriptionLines: JsdocDescriptionLine[],
 *   descriptionStartLine?: Integer,
 *   hasPreterminalDescription: 0|1,
 *   hasPreterminalTagDescription?: 1,
 *   initial: string,
 *   inlineTags: JsdocInlineTag[]
 *   lastDescriptionLine?: Integer,
 *   endLine: Integer,
 *   lineEnd: string,
 *   postDelimiter: string,
 *   tags: JsdocTag[],
 *   terminal: string,
 *   type: "JsdocBlock",
 * }} JsdocBlock
 */

/**
 * @param {object} cfg
 * @param {string} cfg.text
 * @param {string} cfg.tag
 * @param {'pipe' | 'plain' | 'prefix' | 'space'} cfg.format
 * @param {string} cfg.namepathOrURL
 * @returns {JsdocInlineTag}
 */
const inlineTagToAST = ({
  text,
  tag,
  format,
  namepathOrURL
}) => ({
  text,
  tag,
  format,
  namepathOrURL,
  type: 'JsdocInlineTag'
});

/**
 * Converts comment parser AST to ESTree format.
 * @param {import('./index.js').JsdocBlockWithInline} jsdoc
 * @param {import('jsdoc-type-pratt-parser').ParseMode} mode
 * @param {object} opts
 * @param {boolean} [opts.throwOnTypeParsingErrors=false]
 * @returns {JsdocBlock}
 */
const commentParserToESTree = (jsdoc, mode, {
  throwOnTypeParsingErrors = false
} = {}) => {
  /**
   * Strips brackets from a tag's `rawType` values and adds `parsedType`
   * @param {JsdocTag} lastTag
   * @returns {void}
   */
  const cleanUpLastTag = lastTag => {
    // Strip out `}` that encapsulates and is not part of
    //   the type
    stripEncapsulatingBrackets(lastTag);
    if (lastTag.typeLines.length) {
      stripEncapsulatingBrackets(lastTag.typeLines, true);
    }

    // With even a multiline type now in full, add parsing
    let parsedType = null;
    try {
      parsedType = jsdocTypePrattParser.parse(lastTag.rawType, mode);
    } catch (err) {
      // Ignore
      if (lastTag.rawType && throwOnTypeParsingErrors) {
        /** @type {Error} */err.message = `Tag @${lastTag.tag} with raw type ` + `\`${lastTag.rawType}\` had parsing error: ${
        /** @type {Error} */err.message}`;
        throw err;
      }
    }
    lastTag.parsedType = parsedType;
  };
  const {
    source,
    inlineTags: blockInlineTags
  } = jsdoc;
  const {
    tokens: {
      delimiter: delimiterRoot,
      lineEnd: lineEndRoot,
      postDelimiter: postDelimiterRoot,
      start: startRoot,
      end: endRoot
    }
  } = source[0];
  const endLine = source.length - 1;

  /** @type {JsdocBlock} */
  const ast = {
    delimiter: delimiterRoot,
    description: '',
    descriptionLines: [],
    inlineTags: blockInlineTags.map(t => inlineTagToAST(t)),
    initial: startRoot,
    tags: [],
    // `terminal` will be overwritten if there are other entries
    terminal: endRoot,
    hasPreterminalDescription: 0,
    endLine,
    postDelimiter: postDelimiterRoot,
    lineEnd: lineEndRoot,
    type: 'JsdocBlock'
  };

  /**
   * @type {JsdocTag[]}
   */
  const tags = [];

  /** @type {Integer|undefined} */
  let lastDescriptionLine;

  /** @type {JsdocTag|null} */
  let lastTag = null;
  let descLineStateOpen = true;
  source.forEach((info, idx) => {
    const {
      tokens
    } = info;
    const {
      delimiter,
      description,
      postDelimiter,
      start: initial,
      tag,
      end,
      type: rawType
    } = tokens;
    if (!tag && description && descLineStateOpen) {
      if (ast.descriptionStartLine === undefined) {
        ast.descriptionStartLine = idx;
      }
      ast.descriptionEndLine = idx;
    }
    if (tag || end) {
      descLineStateOpen = false;
      if (lastDescriptionLine === undefined) {
        lastDescriptionLine = idx;
      }

      // Clean-up with last tag before end or new tag
      if (lastTag) {
        cleanUpLastTag(lastTag);
      }

      // Stop the iteration when we reach the end
      // but only when there is no tag earlier in the line
      // to still process
      if (end && !tag) {
        ast.terminal = end;
        if (description) {
          if (lastTag) {
            ast.hasPreterminalTagDescription = 1;
          } else {
            ast.hasPreterminalDescription = 1;
          }
          const holder = lastTag || ast;
          holder.description += (holder.description ? '\n' : '') + description;
          holder.descriptionLines.push({
            delimiter,
            description,
            postDelimiter,
            initial,
            type: 'JsdocDescriptionLine'
          });
        }
        return;
      }
      const {
        end: ed,
        delimiter: de,
        postDelimiter: pd,
        start: init,
        ...tkns
      } = tokens;
      if (!tokens.name) {
        let i = 1;
        while (source[idx + i]) {
          const {
            tokens: {
              name,
              postName,
              postType,
              tag: tg
            }
          } = source[idx + i];
          if (tg) {
            break;
          }
          if (name) {
            tkns.postType = postType;
            tkns.name = name;
            tkns.postName = postName;
            break;
          }
          i++;
        }
      }

      /**
       * @type {JsdocInlineTag[]}
       */
      let tagInlineTags = [];
      if (tag) {
        // Assuming the tags from `source` are in the same order as `jsdoc.tags`
        // we can use the `tags` length as index into the parser result tags.
        tagInlineTags =
        /**
         * @type {import('comment-parser').Spec & {
         *   inlineTags: JsdocInlineTagNoType[]
         * }}
         */
        jsdoc.tags[tags.length].inlineTags.map(t => inlineTagToAST(t));
      }

      /** @type {JsdocTag} */
      const tagObj = {
        ...tkns,
        initial: endLine ? init : '',
        postDelimiter: lastDescriptionLine ? pd : '',
        delimiter: lastDescriptionLine ? de : '',
        descriptionLines: [],
        inlineTags: tagInlineTags,
        parsedType: null,
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
      /** @type {JsdocTag} */
      lastTag.typeLines.push( /** @type {JsdocTag} */lastTag.typeLines.length ? {
        delimiter,
        postDelimiter,
        rawType,
        initial,
        type: 'JsdocTypeLine'
      } : {
        delimiter: '',
        postDelimiter: '',
        rawType,
        initial: '',
        type: 'JsdocTypeLine'
      });
      /** @type {JsdocTag} */
      lastTag.rawType += /** @type {JsdocTag} */lastTag.rawType ? '\n' + rawType : rawType;
    }
    if (description) {
      const holder = lastTag || ast;
      holder.descriptionLines.push(holder.descriptionLines.length ? {
        delimiter,
        description,
        postDelimiter,
        initial,
        type: 'JsdocDescriptionLine'
      } : lastTag ? {
        delimiter: '',
        description,
        postDelimiter: '',
        initial: '',
        type: 'JsdocDescriptionLine'
      } : {
        delimiter,
        description,
        postDelimiter,
        initial,
        type: 'JsdocDescriptionLine'
      });
      if (!tag) {
        holder.description += !holder.description && !lastTag ? description : '\n' + description;
      }
    }

    // Clean-up where last line itself has tag content
    if (end && tag) {
      ast.terminal = end;
      ast.hasPreterminalTagDescription = 1;
      cleanUpLastTag( /** @type {JsdocTag} */lastTag);
    }
  });
  ast.lastDescriptionLine = lastDescriptionLine;
  ast.tags = tags;
  return ast;
};
const jsdocVisitorKeys = {
  JsdocBlock: ['descriptionLines', 'tags', 'inlineTags'],
  JsdocDescriptionLine: [],
  JsdocTypeLine: [],
  JsdocTag: ['parsedType', 'typeLines', 'descriptionLines', 'inlineTags'],
  JsdocInlineTag: []
};

/**
 * @typedef {import('./index.js').CommentHandler} CommentHandler
 */

/**
 * @param {{[name: string]: any}} settings
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
    const selector = esquery.parse(commentSelector);
    const ast = commentParserToESTree(jsdoc, mode);
    const _ast = /** @type {unknown} */ast;
    return esquery.matches( /** @type {import('estree').Node} */
    _ast, selector, null, {
      visitorKeys: {
        ...jsdocTypePrattParser.visitorKeys,
        ...jsdocVisitorKeys
      }
    });
  };
};

/**
 * @param {string} str
 * @returns {string}
 */
const toCamelCase = str => {
  return str.toLowerCase().replaceAll(/^[a-z]/gu, init => {
    return init.toUpperCase();
  }).replaceAll(/_(?<wordInit>[a-z])/gu, (_, n1, o, s, {
    wordInit
  }) => {
    return wordInit.toUpperCase();
  });
};

/**
 * @param {RegExpMatchArray & {
 *   indices: {
 *     groups: {
 *       [key: string]: [number, number]
 *     }
 *   }
 *   groups: {[key: string]: string}
 * }} match An inline tag regexp match.
 * @returns {'pipe' | 'plain' | 'prefix' | 'space'}
 */
function determineFormat(match) {
  const {
    separator,
    text
  } = match.groups;
  const [, textEnd] = match.indices.groups.text;
  const [tagStart] = match.indices.groups.tag;
  if (!text) {
    return 'plain';
  } else if (separator === '|') {
    return 'pipe';
  } else if (textEnd < tagStart) {
    return 'prefix';
  }
  return 'space';
}

/**
 * @typedef {import('./index.js').InlineTag} InlineTag
 */

/**
 * Extracts inline tags from a description.
 * @param {string} description
 * @returns {InlineTag[]} Array of inline tags from the description.
 */
function parseDescription(description) {
  /** @type {InlineTag[]} */
  const result = [];

  // This could have been expressed in a single pattern,
  // but having two avoids a potentially exponential time regex.

  // eslint-disable-next-line prefer-regex-literals -- Need 'd' (indices) flag
  const prefixedTextPattern = new RegExp(/(?:\[(?<text>[^\]]+)\])\{@(?<tag>[^}\s]+)\s?(?<namepathOrURL>[^}\s|]*)\}/gu, 'gud');
  // The pattern used to match for text after tag uses a negative lookbehind
  // on the ']' char to avoid matching the prefixed case too.
  // eslint-disable-next-line prefer-regex-literals -- Need 'd' (indices) flag
  const suffixedAfterPattern = new RegExp(/(?<!\])\{@(?<tag>[^}\s]+)\s?(?<namepathOrURL>[^}\s|]*)\s*(?<separator>[\s|])?\s*(?<text>[^}]*)\}/gu, 'gud');
  const matches = [...description.matchAll(prefixedTextPattern), ...description.matchAll(suffixedAfterPattern)];
  for (const mtch of matches) {
    const match =
    /**
    * @type {RegExpMatchArray & {
    *   indices: {
    *     groups: {
    *       [key: string]: [number, number]
    *     }
    *   }
    *   groups: {[key: string]: string}
    * }}
    */
    mtch;
    const {
      tag,
      namepathOrURL,
      text
    } = match.groups;
    const [start, end] = match.indices[0];
    const format = determineFormat(match);
    result.push({
      tag,
      namepathOrURL,
      text,
      format,
      start,
      end
    });
  }
  return result;
}

/**
 * Splits the `{@prefix}` from remaining `Spec.lines[].token.description`
 * into the `inlineTags` tokens, and populates `spec.inlineTags`
 * @param {import('comment-parser').Block} block
 * @returns {import('./index.js').JsdocBlockWithInline}
 */
function parseInlineTags(block) {
  const inlineTags =
  /**
   * @type {(import('./commentParserToESTree.js').JsdocInlineTagNoType & {
   *   line?: import('./commentParserToESTree.js').Integer
   * })[]}
   */
  parseDescription(block.description);

  /** @type {import('./index.js').JsdocBlockWithInline} */
  block.inlineTags = inlineTags;
  for (const tag of block.tags) {
    /**
     * @type {import('./index.js').JsdocTagWithInline}
     */
    tag.inlineTags = parseDescription(tag.description);
  }
  return (
    /**
     * @type {import('./index.js').JsdocBlockWithInline}
     */
    block
  );
}

/* eslint-disable prefer-named-capture-group -- Temporary */
const {
  name: nameTokenizer,
  tag: tagTokenizer,
  type: typeTokenizer,
  description: descriptionTokenizer
} = commentParser.tokenizers;

/**
 * @param {import('comment-parser').Spec} spec
 * @returns {boolean}
 */
const hasSeeWithLink = spec => {
  return spec.tag === 'see' && /\{@link.+?\}/u.test(spec.source[0].source);
};
const defaultNoTypes = ['default', 'defaultvalue', 'description', 'example', 'file', 'fileoverview', 'license', 'overview', 'see', 'summary'];
const defaultNoNames = ['access', 'author', 'default', 'defaultvalue', 'description', 'example', 'exception', 'file', 'fileoverview', 'kind', 'license', 'overview', 'return', 'returns', 'since', 'summary', 'throws', 'version', 'variation'];
const optionalBrackets = /^\[(?<name>[^=]*)=[^\]]*\]/u;
const preserveTypeTokenizer = typeTokenizer('preserve');
const preserveDescriptionTokenizer = descriptionTokenizer('preserve');
const plainNameTokenizer = nameTokenizer();
const getTokenizers = ({
  noTypes = defaultNoTypes,
  noNames = defaultNoNames
} = {}) => {
  // trim
  return [
  // Tag
  tagTokenizer(),
  /**
   * Type tokenizer.
   * @param {import('comment-parser').Spec} spec
   * @returns {import('comment-parser').Spec}
   */
  spec => {
    if (noTypes.includes(spec.tag)) {
      return spec;
    }
    return preserveTypeTokenizer(spec);
  },
  /**
   * Name tokenizer.
   * @param {import('comment-parser').Spec} spec
   * @returns {import('comment-parser').Spec}
   */
  spec => {
    if (spec.tag === 'template') {
      // const preWS = spec.postTag;
      const remainder = spec.source[0].tokens.description;
      const pos = remainder.search(/(?<![\s,])\s/u);
      let name = pos === -1 ? remainder : remainder.slice(0, pos);
      const extra = remainder.slice(pos);
      let postName = '',
        description = '',
        lineEnd = '';
      if (pos > -1) {
        [, postName, description, lineEnd] = /** @type {RegExpMatchArray} */
        extra.match(/(\s*)([^\r]*)(\r)?/u);
      }
      if (optionalBrackets.test(name)) {
        var _name$match, _name$match$groups;
        name =
        /** @type {string} */
        /** @type {RegExpMatchArray} */
        (_name$match = name.match(optionalBrackets)) === null || _name$match === void 0 ? void 0 : (_name$match$groups = _name$match.groups) === null || _name$match$groups === void 0 ? void 0 : _name$match$groups.name;
        spec.optional = true;
      } else {
        spec.optional = false;
      }
      spec.name = name;
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
    return plainNameTokenizer(spec);
  },
  /**
   * Description tokenizer.
   * @param {import('comment-parser').Spec} spec
   * @returns {import('comment-parser').Spec}
   */
  spec => {
    return preserveDescriptionTokenizer(spec);
  }];
};

/**
 * Accepts a comment token and converts it into `comment-parser` AST.
 * @param {{value: string}} commentNode
 * @param {string} [indent=""] Whitespace
 * @returns {import('./index.js').JsdocBlockWithInline}
 */
const parseComment = (commentNode, indent = '') => {
  // Preserve JSDoc block start/end indentation.
  const [block] = commentParser.parse(`${indent}/*${commentNode.value}*/`, {
    // @see https://github.com/yavorskiy/comment-parser/issues/21
    tokenizers: getTokenizers()
  });
  return parseInlineTags(block);
};

/**
 * Obtained originally from {@link https://github.com/eslint/eslint/blob/master/lib/util/source-code.js#L313}.
 *
 * @license MIT
 */

/**
 * @typedef {import('eslint').AST.Token | import('estree').Comment | {
 *   type: import('eslint').AST.TokenType|"Line"|"Block"|"Shebang",
 *   range: [number, number],
 *   value: string
 * }} Token
 */

/**
 * @typedef {import('eslint').Rule.Node|
 *   import('@typescript-eslint/types').TSESTree.Node} ESLintOrTSNode
 */

/**
 * @typedef {number} int
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
 * @param {(import('estree').Comment|import('eslint').Rule.Node) & {
 *   declaration?: any,
 *   decorators?: any[],
 *   parent?: import('eslint').Rule.Node & {
 *     decorators?: any[]
 *   }
 * }} node
 * @returns {import('@typescript-eslint/types').TSESTree.Decorator|undefined}
 */
const getDecorator = node => {
  var _node$declaration, _node$declaration$dec, _node$decorators, _node$parent, _node$parent$decorato;
  return (node === null || node === void 0 ? void 0 : (_node$declaration = node.declaration) === null || _node$declaration === void 0 ? void 0 : (_node$declaration$dec = _node$declaration.decorators) === null || _node$declaration$dec === void 0 ? void 0 : _node$declaration$dec[0]) || (node === null || node === void 0 ? void 0 : (_node$decorators = node.decorators) === null || _node$decorators === void 0 ? void 0 : _node$decorators[0]) || (node === null || node === void 0 ? void 0 : (_node$parent = node.parent) === null || _node$parent === void 0 ? void 0 : (_node$parent$decorato = _node$parent.decorators) === null || _node$parent$decorato === void 0 ? void 0 : _node$parent$decorato[0]);
};

/**
 * Check to see if it is a ES6 export declaration.
 *
 * @param {import('eslint').Rule.Node} astNode An AST node.
 * @returns {boolean} whether the given node represents an export declaration.
 * @private
 */
const looksLikeExport = function (astNode) {
  return astNode.type === 'ExportDefaultDeclaration' || astNode.type === 'ExportNamedDeclaration' || astNode.type === 'ExportAllDeclaration' || astNode.type === 'ExportSpecifier';
};

/**
 * @param {import('eslint').Rule.Node} astNode
 * @returns {import('eslint').Rule.Node}
 */
const getTSFunctionComment = function (astNode) {
  const {
    parent
  } = astNode;
  /* c8 ignore next 3 */
  if (!parent) {
    return astNode;
  }
  const grandparent = parent.parent;
  /* c8 ignore next 3 */
  if (!grandparent) {
    return astNode;
  }
  const greatGrandparent = grandparent.parent;
  const greatGreatGrandparent = greatGrandparent && greatGrandparent.parent;

  // istanbul ignore if
  if ( /** @type {ESLintOrTSNode} */parent.type !== 'TSTypeAnnotation') {
    return astNode;
  }
  switch ( /** @type {ESLintOrTSNode} */grandparent.type) {
    // @ts-expect-error
    case 'PropertyDefinition':
    case 'ClassProperty':
    case 'TSDeclareFunction':
    case 'TSMethodSignature':
    case 'TSPropertySignature':
      return grandparent;
    case 'ArrowFunctionExpression':
      /* c8 ignore next 3 */
      if (!greatGrandparent) {
        return astNode;
      }
      // istanbul ignore else
      if (greatGrandparent.type === 'VariableDeclarator'

      // && greatGreatGrandparent.parent.type === 'VariableDeclaration'
      ) {
        /* c8 ignore next 3 */
        if (!greatGreatGrandparent || !greatGreatGrandparent.parent) {
          return astNode;
        }
        return greatGreatGrandparent.parent;
      }

      // istanbul ignore next
      return astNode;
    case 'FunctionExpression':
      /* c8 ignore next 3 */
      if (!greatGreatGrandparent) {
        return astNode;
      }
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
  }

  /* c8 ignore next 3 */
  if (!greatGreatGrandparent) {
    return astNode;
  }

  // istanbul ignore next
  switch (greatGrandparent.type) {
    case 'ArrowFunctionExpression':
      // istanbul ignore else
      if (greatGreatGrandparent.type === 'VariableDeclarator' && greatGreatGrandparent.parent.type === 'VariableDeclaration') {
        return greatGreatGrandparent.parent;
      }

      // istanbul ignore next
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
const allowableCommentNode = new Set(['AssignmentPattern', 'VariableDeclaration', 'ExpressionStatement', 'MethodDefinition', 'Property', 'ObjectProperty', 'ClassProperty', 'PropertyDefinition', 'ExportDefaultDeclaration', 'ReturnStatement']);

/**
 * Reduces the provided node to the appropriate node for evaluating
 * JSDoc comment status.
 *
 * @param {import('eslint').Rule.Node} node An AST node.
 * @param {import('eslint').SourceCode} sourceCode The ESLint SourceCode.
 * @returns {import('eslint').Rule.Node} The AST node that
 *   can be evaluated for appropriate JSDoc comments.
 */
const getReducedASTNode = function (node, sourceCode) {
  let {
    parent
  } = node;
  switch ( /** @type {ESLintOrTSNode} */node.type) {
    case 'TSFunctionType':
      return getTSFunctionComment(node);
    case 'TSInterfaceDeclaration':
    case 'TSTypeAliasDeclaration':
    case 'TSEnumDeclaration':
    case 'ClassDeclaration':
    case 'FunctionDeclaration':
      /* c8 ignore next 3 */
      if (!parent) {
        return node;
      }
      return looksLikeExport(parent) ? parent : node;
    case 'TSDeclareFunction':
    case 'ClassExpression':
    case 'ObjectExpression':
    case 'ArrowFunctionExpression':
    case 'TSEmptyBodyFunctionExpression':
    case 'FunctionExpression':
      /* c8 ignore next 3 */
      if (!parent) {
        return node;
      }
      if (!invokedExpression.has(parent.type)) {
        /**
         * @type {import('eslint').Rule.Node|Token|null}
         */
        let token = node;
        do {
          token = sourceCode.getTokenBefore( /** @type {import('eslint').Rule.Node|import('eslint').AST.Token} */
          token, {
            includeComments: true
          });
        } while (token && token.type === 'Punctuator' && token.value === '(');
        if (token && token.type === 'Block') {
          return node;
        }
        if (sourceCode.getCommentsBefore(node).length) {
          return node;
        }
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
 * @param {import('eslint').Rule.Node} astNode The AST node to get
 *   the comment for.
 * @param {import('eslint').SourceCode} sourceCode
 * @param {{maxLines: int, minLines: int, [name: string]: any}} settings
 * @returns {Token|null} The Block comment token containing the JSDoc comment
 *    for the given node or null if not found.
 * @private
 */
const findJSDocComment = (astNode, sourceCode, settings) => {
  const {
    minLines,
    maxLines
  } = settings;

  /** @type {import('eslint').Rule.Node|import('estree').Comment} */
  let currentNode = astNode;
  let tokenBefore = null;
  while (currentNode) {
    const decorator = getDecorator(currentNode);
    if (decorator) {
      const dec = /** @type {unknown} */decorator;
      currentNode = /** @type {import('eslint').Rule.Node} */dec;
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

  /* c8 ignore next 3 */
  if (!tokenBefore || !currentNode.loc || !tokenBefore.loc) {
    return null;
  }
  if (tokenBefore.type === 'Block' && /^\*\s/u.test(tokenBefore.value) && currentNode.loc.start.line - tokenBefore.loc.end.line >= minLines && currentNode.loc.start.line - tokenBefore.loc.end.line <= maxLines) {
    return tokenBefore;
  }
  return null;
};

/**
 * Retrieves the JSDoc comment for a given node.
 *
 * @param {import('eslint').SourceCode} sourceCode The ESLint SourceCode
 * @param {import('eslint').Rule.Node} node The AST node to get
 *   the comment for.
 * @param {{maxLines: int, minLines: int, [name: string]: any}} settings The
 *   settings in context
 * @returns {Token|null} The Block comment
 *   token containing the JSDoc comment for the given node or
 *   null if not found.
 * @public
 */
const getJSDocComment = function (sourceCode, node, settings) {
  const reducedNode = getReducedASTNode(node, sourceCode);
  return findJSDocComment(reducedNode, sourceCode, settings);
};

/**
 * @typedef {import('./index.js').ESTreeToStringOptions} ESTreeToStringOptions
 */

const stringifiers = {
  /**
   * @param {import('./commentParserToESTree.js').JsdocBlock} node
   * @param {ESTreeToStringOptions} opts
   * @param {string[]} descriptionLines
   * @param {string[]} tags
   * @returns {string}
   */
  JsdocBlock({
    delimiter,
    postDelimiter,
    lineEnd,
    initial,
    terminal,
    endLine
  }, opts, descriptionLines, tags) {
    var _descriptionLines$at, _tags$at;
    const alreadyHasLine = descriptionLines.length && !tags.length && ((_descriptionLines$at = descriptionLines.at(-1)) === null || _descriptionLines$at === void 0 ? void 0 : _descriptionLines$at.endsWith('\n')) || tags.length && ((_tags$at = tags.at(-1)) === null || _tags$at === void 0 ? void 0 : _tags$at.endsWith('\n'));
    return `${initial}${delimiter}${postDelimiter}${endLine ? `
` : ''}${
    // Could use `node.description` (and `node.lineEnd`), but lines may have
    //   been modified
    descriptionLines.length ? descriptionLines.join(lineEnd + '\n') + (tags.length ? lineEnd + '\n' : '') : ''}${tags.length ? tags.join(lineEnd + '\n') : ''}${endLine && !alreadyHasLine ? `${lineEnd}
 ${initial}` : endLine ? ` ${initial}` : ''}${terminal}`;
  },
  /**
   * @param {import('./commentParserToESTree.js').JsdocDescriptionLine} node
   * @returns {string}
   */
  JsdocDescriptionLine({
    initial,
    delimiter,
    postDelimiter,
    description
  }) {
    return `${initial}${delimiter}${postDelimiter}${description}`;
  },
  /**
   * @param {import('./commentParserToESTree.js').JsdocTypeLine} node
   * @returns {string}
   */
  JsdocTypeLine({
    initial,
    delimiter,
    postDelimiter,
    rawType
  }) {
    return `${initial}${delimiter}${postDelimiter}${rawType}`;
  },
  /**
   * @param {import('./commentParserToESTree.js').JsdocInlineTag} node
   */
  JsdocInlineTag({
    format,
    namepathOrURL,
    tag,
    text
  }) {
    return format === 'pipe' ? `{@${tag} ${namepathOrURL}|${text}}` : format === 'plain' ? `{@${tag} ${namepathOrURL}}` : format === 'prefix' ? `[${text}]{@${tag} ${namepathOrURL}}`
    // "space"
    : `{@${tag} ${namepathOrURL} ${text}}`;
  },
  /**
   * @param {import('./commentParserToESTree.js').JsdocTag} node
   * @param {ESTreeToStringOptions} opts
   * @param {string} parsedType
   * @param {string[]} typeLines
   * @param {string[]} descriptionLines
   * @returns {string}
   */
  JsdocTag(node, opts, parsedType, typeLines, descriptionLines) {
    const {
      description,
      name,
      postName,
      postTag,
      postType,
      initial,
      delimiter,
      postDelimiter,
      tag
      // , rawType
    } = node;
    return `${initial}${delimiter}${postDelimiter}@${tag}${postTag}${
    // Could do `rawType` but may have been changed; could also do
    //   `typeLines` but not as likely to be changed
    // parsedType
    // Comment this out later in favor of `parsedType`
    // We can't use raw `typeLines` as first argument has delimiter on it
    opts.preferRawType || !parsedType ? typeLines.length ? `{${typeLines.join('\n')}}` : '' : parsedType}${postType}${name ? `${name}${postName || (description ? '\n' : '')}` : ''}${descriptionLines.join('\n')}`;
  }
};
const visitorKeys = {
  ...jsdocVisitorKeys,
  ...jsdocTypePrattParser.visitorKeys
};

/**
 * @todo convert for use by escodegen (until may be patched to support
 *   custom entries?).
 * @param {import('./commentParserToESTree.js').JsdocBlock|
 *   import('./commentParserToESTree.js').JsdocDescriptionLine|
 *   import('./commentParserToESTree.js').JsdocTypeLine|
 *   import('./commentParserToESTree.js').JsdocTag|
 *   import('./commentParserToESTree.js').JsdocInlineTag|
 *   import('jsdoc-type-pratt-parser').RootResult
 * } node
 * @param {ESTreeToStringOptions} opts
 * @throws {Error}
 * @returns {string}
 */
function estreeToString(node, opts = {}) {
  if (Object.prototype.hasOwnProperty.call(stringifiers, node.type)) {
    const childNodeOrArray = visitorKeys[node.type];
    const args = /** @type {(string[]|string|null)[]} */
    childNodeOrArray.map(key => {
      // @ts-expect-error
      return Array.isArray(node[key])
      // @ts-expect-error
      ? node[key].map((
      /**
       * @type {import('./commentParserToESTree.js').JsdocBlock|
       *   import('./commentParserToESTree.js').JsdocDescriptionLine|
       *   import('./commentParserToESTree.js').JsdocTypeLine|
       *   import('./commentParserToESTree.js').JsdocTag|
       *   import('./commentParserToESTree.js').JsdocInlineTag}
       */
      item) => {
        return estreeToString(item, opts);
      })
      // @ts-expect-error
      : node[key] === undefined || node[key] === null ? null
      // @ts-expect-error
      : estreeToString(node[key], opts);
    });
    return stringifiers[
    /**
     * @type {import('./commentParserToESTree.js').JsdocBlock|
     *   import('./commentParserToESTree.js').JsdocDescriptionLine|
     *   import('./commentParserToESTree.js').JsdocTypeLine|
     *   import('./commentParserToESTree.js').JsdocTag}
     */
    node.type](node, opts,
    // @ts-expect-error
    ...args);
  }

  // We use raw type instead but it is a key as other apps may wish to traverse
  if (node.type.startsWith('JsdocType')) {
    return opts.preferRawType ? '' : `{${jsdocTypePrattParser.stringify( /** @type {import('jsdoc-type-pratt-parser').RootResult} */
    node)}}`;
  }
  throw new Error(`Unhandled node type: ${node.type}`);
}

Object.defineProperty(exports, 'jsdocTypeVisitorKeys', {
  enumerable: true,
  get: function () { return jsdocTypePrattParser.visitorKeys; }
});
exports.commentHandler = commentHandler;
exports.commentParserToESTree = commentParserToESTree;
exports.defaultNoNames = defaultNoNames;
exports.defaultNoTypes = defaultNoTypes;
exports.estreeToString = estreeToString;
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
