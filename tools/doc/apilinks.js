'use strict';

// Scan API sources for definitions.
//
// Note the output is produced based on a world class parser, adherence to
// conventions, and a bit of guess work. Examples:
//
//  * We scan for top level module.exports statements, and determine what
//    is exported by looking at the source code only (i.e., we don't do
//    an eval). If exports include `Foo`, it probably is a class, whereas
//    if what is exported is `constants` it probably is prefixed by the
//    basename of the source file (e.g., `zlib`), unless that source file is
//    `buffer.js`, in which case the name is just `buf`.  unless the constant
//    is `kMaxLength`, in which case it is `buffer`.
//
//  * We scan for top level definitions for those exports, handling
//    most common cases (e.g., `X.prototype.foo =`, `X.foo =`,
//    `function X(...) {...}`). Over time, we expect to handle more
//    cases (example: ES2015 class definitions).

const acorn = require('../../deps/acorn');
const fs = require('fs');
const path = require('path');
const child_process = require('child_process');

// Determine orign repo and most recent commit.
const repo = (
  child_process.execSync(
    'git config remote.origin.url',
    { stdio: ['ignore', null, 'ignore'] }
  ).toString()
 .match(/(\w+\/\w+)\.git\r?\n?$/) || ['', 'nodejs/node'])[1];

let hash = child_process.execSync(
  'git log -1 --pretty=%H',
  { stdio: ['ignore', null, 'ignore'] }
).toString().trim() || 'master';

try {
  // Replace hash with tag name, if tagged.
  hash = child_process.execSync(
    `git describe --contains ${hash}`,
    { stdio: ['ignore', null, 'ignore'] }
  ).toString();
} catch {
}

const definition = {};
process.argv.slice(2).forEach((file) => {
  const basename = path.basename(file, '.js');

  // Parse source.
  const source = fs.readFileSync(file, 'utf8');
  const ast = acorn.parse(source, { ecmaVersion: 10, locations: true });
  const program = ast.body;

  // Scan for exports.
  const exported = { constructors: [], identifiers: [] };
  program.forEach((statement) => {
    if (statement.type !== 'ExpressionStatement') return;
    const expr = statement.expression;
    if (expr.type !== 'AssignmentExpression') return;

    let lhs = expr.left;
    if (expr.left.object.type === 'MemberExpression') lhs = lhs.object;
    if (lhs.type !== 'MemberExpression') return;
    if (lhs.object.name !== 'module') return;
    if (lhs.property.name !== 'exports') return;

    let rhs = expr.right;
    while (rhs.type === 'AssignmentExpression') rhs = rhs.right;

    if (rhs.type === 'NewExpression') {
      exported.constructors.push(rhs.callee.name);
    } else if (rhs.type === 'ObjectExpression') {
      rhs.properties.forEach((property) => {
        if (property.value.type === 'Identifier') {
          exported.identifiers.push(property.value.name);
          if (/^[A-Z]/.test(property.value.name[0])) {
            exported.constructors.push(property.value.name);
          }
        }
      });
    } else if (rhs.type === 'Identifier') {
      exported.identifiers.push(rhs.name);
    }
  });

  // Scan for definitions matching those exports; currently supports:
  //
  //   ClassName.foo = ...;
  //   ClassName.prototype.foo = ...;
  //   function Identifier(...) {...};
  //
  const link = `https://github.com/${repo}/blob/${hash}/${file}`;
  program.forEach((statement) => {
    if (statement.type === 'ExpressionStatement') {
      const expr = statement.expression;
      if (expr.type !== 'AssignmentExpression') return;
      if (expr.left.type !== 'MemberExpression') return;

      let object;
      if (expr.left.object.type === 'MemberExpression') {
        if (expr.left.object.property.name !== 'prototype') return;
        object = expr.left.object.object;
      } else if (expr.left.object.type === 'Identifier') {
        object = expr.left.object;
      } else {
        return;
      }

      if (!exported.constructors.includes(object.name)) return;

      let objectName = object.name;
      if (expr.left.object.type === 'MemberExpression') {
        objectName = objectName.toLowerCase();
        if (objectName === 'buffer') objectName = 'buf';
      }

      let name = expr.left.property.name;
      if (expr.left.computed) {
        name = `${objectName}[${name}]`;
      } else {
        name = `${objectName}.${name}`;
      }

      definition[name] = `${link}#L${statement.start.line}`;
    } else if (statement.type === 'FunctionDeclaration') {
      const name = statement.id.name;
      if (!exported.identifiers.includes(name)) return;
      if (basename.startsWith('_')) return;
      definition[`${basename}.${name}`] = `${link}#L${statement.start.line}`;
    }
  });
});

console.log(JSON.stringify(definition, null, 2));
