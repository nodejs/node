'use strict';

const acorn = require('internal/deps/acorn/dist/acorn');
const ESLoader = require('internal/loader/singleton');

/*
Performs importing from static import statments

"a(); import { x, y } from 'some_modules'; x(); y"

would become:

"a(); x(); y"

The symbol is a promise from Promise.all on an array of promises
from Loader#import. When a promise finishes it attaches to
`repl.context` with a getter, so that the imported names behave
like variables not values (live binding).
*/

function processTopLevelImport(src, repl) {
  let root;
  try {
    root = acorn.parse(src, { ecmaVersion: 8, sourceType: 'module' });
  } catch (err) {
    return null;
  }

  const importLength =
    root.body.filter((n) => n.type === 'ImportDeclaration').length;

  if (importLength === 0)
    return null;

  const importPromises = [];
  let newBody = '';

  for (const index in root.body) {
    const node = root.body[index];
    if (node.type === 'ImportDeclaration') {
      const lPromise = ESLoader.loader.import(node.source.value)
        .then((exports) => {
          const { specifiers } = node;
          if (specifiers[0].type === 'ImportNamespaceSpecifier') {
            Object.defineProperty(repl.context, specifiers[0].local.name, {
              enumerable: true,
              configurable: true,
              get() { return exports; }
            });
          } else {
            const properties = {};
            for (const { imported, local } of specifiers) {
              const imp = imported ? imported.name : 'default';
              properties[local.name] = {
                enumerable: true,
                configurable: true,
                get() { return exports[imp]; }
              };
            }
            Object.defineProperties(repl.context, properties);
          }
        });

      importPromises.push(lPromise);
    } else if (node.expression !== undefined) {
      const { start, end } = node.expression;
      newBody += src.substring(start, end) + ';';
    } else {
      newBody += src.substring(node.start, node.end);
    }
  }

  return {
    promise: Promise.all(importPromises),
    body: newBody,
  };
}

module.exports = processTopLevelImport;
