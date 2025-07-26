'use strict';

const {
  ArrayPrototypeForEach,
  ArrayPrototypeIncludes,
  ArrayPrototypeJoin,
  ArrayPrototypePop,
  ArrayPrototypePush,
  FunctionPrototype,
  ObjectKeys,
  RegExpPrototypeSymbolReplace,
  StringPrototypeEndsWith,
  StringPrototypeIncludes,
  StringPrototypeIndexOf,
  StringPrototypeRepeat,
  StringPrototypeSplit,
  StringPrototypeStartsWith,
  SyntaxError,
} = primordials;

const parser = require('internal/deps/acorn/acorn/dist/acorn').Parser;
const walk = require('internal/deps/acorn/acorn-walk/dist/walk');
const { Recoverable } = require('internal/repl');

function isTopLevelDeclaration(state) {
  return state.ancestors[state.ancestors.length - 2] === state.body;
}

const noop = FunctionPrototype;
const visitorsWithoutAncestors = {
  ClassDeclaration(node, state, c) {
    if (isTopLevelDeclaration(state)) {
      state.prepend(node, `${node.id.name}=`);
      ArrayPrototypePush(
        state.hoistedDeclarationStatements,
        `let ${node.id.name}; `,
      );
    }

    walk.base.ClassDeclaration(node, state, c);
  },
  ForOfStatement(node, state, c) {
    if (node.await === true) {
      state.containsAwait = true;
    }
    walk.base.ForOfStatement(node, state, c);
  },
  FunctionDeclaration(node, state, c) {
    state.prepend(node, `this.${node.id.name} = ${node.id.name}; `);
    ArrayPrototypePush(
      state.hoistedDeclarationStatements,
      `var ${node.id.name}; `,
    );
  },
  FunctionExpression: noop,
  ArrowFunctionExpression: noop,
  MethodDefinition: noop,
  AwaitExpression(node, state, c) {
    state.containsAwait = true;
    walk.base.AwaitExpression(node, state, c);
  },
  ReturnStatement(node, state, c) {
    state.containsReturn = true;
    walk.base.ReturnStatement(node, state, c);
  },
  VariableDeclaration(node, state, c) {
    const variableKind = node.kind;
    const isIterableForDeclaration = ArrayPrototypeIncludes(
      ['ForOfStatement', 'ForInStatement'],
      state.ancestors[state.ancestors.length - 2].type,
    );

    if (variableKind === 'var' || isTopLevelDeclaration(state)) {
      state.replace(
        node.start,
        node.start + variableKind.length + (isIterableForDeclaration ? 1 : 0),
        variableKind === 'var' && isIterableForDeclaration ?
          '' :
          'void' + (node.declarations.length === 1 ? '' : ' ('),
      );

      if (!isIterableForDeclaration) {
        ArrayPrototypeForEach(node.declarations, (decl) => {
          state.prepend(decl, '(');
          state.append(decl, decl.init ? ')' : '=undefined)');
        });

        if (node.declarations.length !== 1) {
          state.append(node.declarations[node.declarations.length - 1], ')');
        }
      }

      const variableIdentifiersToHoist = [
        ['var', []],
        ['let', []],
      ];
      function registerVariableDeclarationIdentifiers(node) {
        switch (node.type) {
          case 'Identifier':
            ArrayPrototypePush(
              variableIdentifiersToHoist[variableKind === 'var' ? 0 : 1][1],
              node.name,
            );
            break;
          case 'ObjectPattern':
            ArrayPrototypeForEach(node.properties, (property) => {
              registerVariableDeclarationIdentifiers(property.value || property.argument);
            });
            break;
          case 'ArrayPattern':
            ArrayPrototypeForEach(node.elements, (element) => {
              registerVariableDeclarationIdentifiers(element);
            });
            break;
        }
      }

      ArrayPrototypeForEach(node.declarations, (decl) => {
        registerVariableDeclarationIdentifiers(decl.id);
      });

      ArrayPrototypeForEach(
        variableIdentifiersToHoist,
        ({ 0: kind, 1: identifiers }) => {
          if (identifiers.length > 0) {
            ArrayPrototypePush(
              state.hoistedDeclarationStatements,
              `${kind} ${ArrayPrototypeJoin(identifiers, ', ')}; `,
            );
          }
        },
      );
    }

    walk.base.VariableDeclaration(node, state, c);
  },
};

const visitors = {};
for (const nodeType of ObjectKeys(walk.base)) {
  const callback = visitorsWithoutAncestors[nodeType] || walk.base[nodeType];
  visitors[nodeType] = (node, state, c) => {
    const isNew = node !== state.ancestors[state.ancestors.length - 1];
    if (isNew) {
      ArrayPrototypePush(state.ancestors, node);
    }
    callback(node, state, c);
    if (isNew) {
      ArrayPrototypePop(state.ancestors);
    }
  };
}

function processTopLevelAwait(src) {
  const wrapPrefix = '(async () => { ';
  const wrapped = `${wrapPrefix}${src} })()`;
  const wrappedArray = StringPrototypeSplit(wrapped, '');
  let root;
  try {
    root = parser.parse(wrapped, { ecmaVersion: 'latest' });
  } catch (e) {
    if (StringPrototypeStartsWith(e.message, 'Unterminated '))
      throw new Recoverable(e);
    // If the parse error is before the first "await", then use the execution
    // error. Otherwise we must emit this parse error, making it look like a
    // proper syntax error.
    const awaitPos = StringPrototypeIndexOf(src, 'await');
    const errPos = e.pos - wrapPrefix.length;
    if (awaitPos > errPos)
      return null;
    // Convert keyword parse errors on await into their original errors when
    // possible.
    if (errPos === awaitPos + 6 &&
        StringPrototypeIncludes(e.message, 'Expecting Unicode escape sequence'))
      return null;
    if (errPos === awaitPos + 7 &&
        StringPrototypeIncludes(e.message, 'Unexpected token'))
      return null;
    const line = e.loc.line;
    const column = line === 1 ? e.loc.column - wrapPrefix.length : e.loc.column;
    let message = '\n' + StringPrototypeSplit(src, '\n', line)[line - 1] + '\n' +
        StringPrototypeRepeat(' ', column) +
        '^\n\n' + RegExpPrototypeSymbolReplace(/ \([^)]+\)/, e.message, '');
    // V8 unexpected token errors include the token string.
    if (StringPrototypeEndsWith(message, 'Unexpected token'))
      message += " '" +
        // Wrapper end may cause acorn to report error position after the source
        (src[e.pos - wrapPrefix.length] ?? src[src.length - 1]) +
        "'";
    // eslint-disable-next-line no-restricted-syntax
    throw new SyntaxError(message);
  }
  const body = root.body[0].expression.callee.body;
  const state = {
    body,
    ancestors: [],
    hoistedDeclarationStatements: [],
    replace(from, to, str) {
      for (let i = from; i < to; i++) {
        wrappedArray[i] = '';
      }
      if (from === to) str += wrappedArray[from];
      wrappedArray[from] = str;
    },
    prepend(node, str) {
      wrappedArray[node.start] = str + wrappedArray[node.start];
    },
    append(node, str) {
      wrappedArray[node.end - 1] += str;
    },
    containsAwait: false,
    containsReturn: false,
  };

  walk.recursive(body, state, visitors);

  // Do not transform if
  // 1. False alarm: there isn't actually an await expression.
  // 2. There is a top-level return, which is not allowed.
  if (!state.containsAwait || state.containsReturn) {
    return null;
  }

  for (let i = body.body.length - 1; i >= 0; i--) {
    const node = body.body[i];
    if (node.type === 'EmptyStatement') continue;
    if (node.type === 'ExpressionStatement') {
      // For an expression statement of the form
      // ( expr ) ;
      // ^^^^^^^^^^   // node
      //   ^^^^       // node.expression
      //
      // We do not want the left parenthesis before the `return` keyword;
      // therefore we prepend the `return (` to `node`.
      //
      // On the other hand, we do not want the right parenthesis after the
      // semicolon. Since there can only be more right parentheses between
      // node.expression.end and the semicolon, appending one more to
      // node.expression should be fine.
      //
      // We also create a wrapper object around the result of the expression.
      // Consider an expression of the form `(await x).y`. If we just return
      // this expression from an async function, the caller will await `y`, too,
      // if it evaluates to a Promise. Instead, we return
      // `{ value: ((await x).y) }`, which allows the caller to retrieve the
      // awaited value correctly.
      state.prepend(node.expression, '{ value: (');
      state.prepend(node, 'return ');
      state.append(node.expression, ') }');
    }
    break;
  }

  return (
    ArrayPrototypeJoin(state.hoistedDeclarationStatements, '') +
    ArrayPrototypeJoin(wrappedArray, '')
  );
}

module.exports = {
  processTopLevelAwait,
};
