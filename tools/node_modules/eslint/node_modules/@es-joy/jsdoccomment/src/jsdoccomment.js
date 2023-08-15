/* eslint-disable jsdoc/imports-as-dependencies -- https://github.com/gajus/eslint-plugin-jsdoc/issues/1114 */
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
const isCommentToken = (token) => {
  return token.type === 'Line' || token.type === 'Block' ||
    token.type === 'Shebang';
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
const getDecorator = (node) => {
  return node?.declaration?.decorators?.[0] || node?.decorators?.[0] ||
      node?.parent?.decorators?.[0];
};

/**
 * Check to see if it is a ES6 export declaration.
 *
 * @param {import('eslint').Rule.Node} astNode An AST node.
 * @returns {boolean} whether the given node represents an export declaration.
 * @private
 */
const looksLikeExport = function (astNode) {
  return astNode.type === 'ExportDefaultDeclaration' ||
    astNode.type === 'ExportNamedDeclaration' ||
    astNode.type === 'ExportAllDeclaration' ||
    astNode.type === 'ExportSpecifier';
};

/**
 * @param {import('eslint').Rule.Node} astNode
 * @returns {import('eslint').Rule.Node}
 */
const getTSFunctionComment = function (astNode) {
  const {parent} = astNode;
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
  if (/** @type {ESLintOrTSNode} */ (parent).type !== 'TSTypeAnnotation') {
    return astNode;
  }

  switch (/** @type {ESLintOrTSNode} */ (grandparent).type) {
  // @ts-expect-error
  case 'PropertyDefinition': case 'ClassProperty':
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
    if (
      greatGrandparent.type === 'VariableDeclarator'

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
    if (
      greatGreatGrandparent.type === 'VariableDeclarator' &&
      greatGreatGrandparent.parent.type === 'VariableDeclaration'
    ) {
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

const invokedExpression = new Set(
  ['CallExpression', 'OptionalCallExpression', 'NewExpression']
);
const allowableCommentNode = new Set([
  'AssignmentPattern',
  'VariableDeclaration',
  'ExpressionStatement',
  'MethodDefinition',
  'Property',
  'ObjectProperty',
  'ClassProperty',
  'PropertyDefinition',
  'ExportDefaultDeclaration',
  'ReturnStatement'
]);

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
  let {parent} = node;

  switch (/** @type {ESLintOrTSNode} */ (node).type) {
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
    if (
      !invokedExpression.has(parent.type)
    ) {
      /**
       * @type {import('eslint').Rule.Node|Token|null}
       */
      let token = node;
      do {
        token = sourceCode.getTokenBefore(
          /** @type {import('eslint').Rule.Node|import('eslint').AST.Token} */ (
            token
          ),
          {includeComments: true}
        );
      } while (token && token.type === 'Punctuator' && token.value === '(');

      if (token && token.type === 'Block') {
        return node;
      }

      if (sourceCode.getCommentsBefore(node).length) {
        return node;
      }

      while (
        !sourceCode.getCommentsBefore(parent).length &&
        !(/Function/u).test(parent.type) &&
        !allowableCommentNode.has(parent.type)
      ) {
        ({parent} = parent);

        if (!parent) {
          break;
        }
      }
      if (parent && parent.type !== 'FunctionDeclaration' &&
        parent.type !== 'Program'
      ) {
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
  const {minLines, maxLines} = settings;

  /** @type {import('eslint').Rule.Node|import('estree').Comment} */
  let currentNode = astNode;
  let tokenBefore = null;

  while (currentNode) {
    const decorator = getDecorator(currentNode);
    if (decorator) {
      const dec = /** @type {unknown} */ (decorator);
      currentNode = /** @type {import('eslint').Rule.Node} */ (dec);
    }
    tokenBefore = sourceCode.getTokenBefore(
      currentNode, {includeComments: true}
    );
    if (
      tokenBefore && tokenBefore.type === 'Punctuator' &&
      tokenBefore.value === '('
    ) {
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

  if (
    tokenBefore.type === 'Block' &&
    (/^\*\s/u).test(tokenBefore.value) &&
    currentNode.loc.start.line - tokenBefore.loc.end.line >= minLines &&
    currentNode.loc.start.line - tokenBefore.loc.end.line <= maxLines
  ) {
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

export {
  getReducedASTNode, getJSDocComment, getDecorator, findJSDocComment
};
