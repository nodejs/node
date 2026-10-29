/**
 * @file Prefer AbortSignal.abort() for already-aborted signals.
 */
'use strict';

const message = 'Use AbortSignal.abort() instead of creating and aborting an AbortController.';

function isAbortControllerConstruction(node) {
  return node?.type === 'NewExpression' &&
         node.callee.type === 'Identifier' &&
         node.callee.name === 'AbortController' &&
         node.arguments.length === 0;
}

function isIdentifier(node, name) {
  return node?.type === 'Identifier' && node.name === name;
}

function isProperty(node, name) {
  return !node.computed && isIdentifier(node.property, name);
}

function isAbortCallStatement(node, name) {
  const expression = node?.expression;
  const callee = expression?.callee;
  return node?.type === 'ExpressionStatement' &&
         expression.type === 'CallExpression' &&
         callee.type === 'MemberExpression' &&
         isIdentifier(callee.object, name) &&
         isProperty(callee, 'abort') &&
         expression.arguments.length <= 1;
}

function isSignalReference(reference, name) {
  const { identifier } = reference;
  const parent = identifier.parent;
  return isIdentifier(identifier, name) &&
         parent?.type === 'MemberExpression' &&
         parent.object === identifier &&
         isProperty(parent, 'signal');
}

function isAbortReference(reference, abortStatement, name) {
  const { identifier } = reference;
  const parent = identifier.parent;
  return isIdentifier(identifier, name) &&
         parent?.type === 'MemberExpression' &&
         parent.object === identifier &&
         isProperty(parent, 'abort') &&
         parent.parent === abortStatement.expression;
}

module.exports = {
  meta: {
    fixable: 'code',
  },

  create(context) {
    const sourceCode = context.sourceCode;
    const candidates = [];

    function hasCommentsBetween(left, right) {
      return sourceCode.getCommentsBefore(right)
        .some((comment) => comment.range[0] > left.range[1]);
    }

    function rangeIncludingTrailingLine(statement) {
      const tokenAfter = sourceCode.getTokenAfter(statement, { includeComments: true });
      if (tokenAfter && tokenAfter.loc.start.line > statement.loc.end.line) {
        return [statement.range[0], tokenAfter.range[0]];
      }
      return statement.range;
    }

    return {
      VariableDeclarator(node) {
        if (node.id.type !== 'Identifier' ||
            !isAbortControllerConstruction(node.init) ||
            node.parent.declarations.length !== 1) {
          return;
        }

        const variableDeclaration = node.parent;
        const parent = variableDeclaration.parent;
        if (parent.type !== 'BlockStatement' && parent.type !== 'Program') {
          return;
        }

        const index = parent.body.indexOf(variableDeclaration);
        const abortStatement = parent.body[index + 1];
        if (!isAbortCallStatement(abortStatement, node.id.name) ||
            hasCommentsBetween(variableDeclaration, abortStatement)) {
          return;
        }

        candidates.push({
          abortStatement,
          declarator: node,
          variableDeclaration,
        });
      },

      'Program:exit'() {
        for (const { abortStatement, declarator, variableDeclaration } of candidates) {
          const [variable] = sourceCode.scopeManager.getDeclaredVariables(declarator);
          if (!variable) {
            continue;
          }

          const name = declarator.id.name;
          const references = variable.references.filter((reference) => {
            return reference.identifier !== declarator.id;
          });
          const signalReferences = references.filter((reference) => {
            return isSignalReference(reference, name);
          });
          const abortReferences = references.filter((reference) => {
            return isAbortReference(reference, abortStatement, name);
          });

          if (references.length !== (1 + signalReferences.length) ||
              abortReferences.length !== 1) {
            continue;
          }

          const signalNode = signalReferences[0].identifier.parent;
          const abortArguments = abortStatement.expression.arguments;
          const abortReason = abortArguments.length === 0 ?
            '' : sourceCode.getText(abortArguments[0]);

          context.report({
            node: declarator,
            message,
            fix(fixer) {
              const abortSignalCreationCall = `AbortSignal.abort(${abortReason})`;
              if (signalReferences.length > 1) {
                return [
                  fixer.replaceText(declarator.init, abortSignalCreationCall),
                  fixer.removeRange(rangeIncludingTrailingLine(abortStatement)),
                  ...signalReferences.map(({ identifier: { parent } }) => fixer.replaceText(parent, name)),
                ];
              }
              return [
                fixer.removeRange(rangeIncludingTrailingLine(variableDeclaration)),
                fixer.removeRange(rangeIncludingTrailingLine(abortStatement)),
                fixer.replaceText(signalNode, abortSignalCreationCall),
              ];
            },
          });
        }
      },
    };
  },
};
