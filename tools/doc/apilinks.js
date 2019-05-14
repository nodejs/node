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

const acorn = require('../../deps/acorn/acorn');
const fs = require('fs');
const path = require('path');
const child_process = require('child_process');

// Run a command, capturing stdout, ignoring errors.
function execSync(command) {
  try {
    return child_process.execSync(
      command,
      { stdio: ['ignore', null, 'ignore'] }
    ).toString().trim();
  } catch {
    return '';
  }
}

// Determine origin repo and tag (or hash) of the most recent commit.
const localBranch = execSync('git name-rev --name-only HEAD');
const trackingRemote = execSync(`git config branch.${localBranch}.remote`);
const remoteUrl = execSync(`git config remote.${trackingRemote}.url`);
const repo = (remoteUrl.match(/(\w+\/\w+)\.git\r?\n?$/) ||
             ['', 'nodejs/node'])[1];

const hash = execSync('git log -1 --pretty=%H') || 'master';
const tag = execSync(`git describe --contains ${hash}`).split('\n')[0] || hash;

// Extract definitions from each file specified.
const definition = {};
const output = process.argv[2];
const inputs = process.argv.slice(3);
inputs.forEach((file) => {
  const basename = path.basename(file, '.js');

  // Parse source.
  const source = fs.readFileSync(file, 'utf8');
  const ast = acorn.parse(
    source,
    { allowReturnOutsideFunction: true, ecmaVersion: 10, locations: true });
  const program = ast.body;

  // Build link
  const link = `https://github.com/${repo}/blob/${tag}/` +
    path.relative('.', file).replace(/\\/g, '/');

  // Scan for exports.
  const exported = { constructors: [], identifiers: [] };
  const indirect = {};
  program.forEach((statement) => {
    if (statement.type === 'ExpressionStatement') {
      const expr = statement.expression;
      if (expr.type !== 'AssignmentExpression') return;

      let lhs = expr.left;
      if (lhs.type !== 'MemberExpression') return;
      if (lhs.object.type === 'MemberExpression') lhs = lhs.object;
      if (lhs.object.name === 'exports') {
        const name = lhs.property.name;
        if (expr.right.type === 'FunctionExpression') {
          definition[`${basename}.${name}`] =
            `${link}#L${statement.loc.start.line}`;
        } else if (expr.right.type === 'Identifier') {
          if (expr.right.name === name) {
            indirect[name] = `${basename}.${name}`;
          }
        } else {
          exported.identifiers.push(name);
        }
      } else if (lhs.object.name === 'module') {
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
      }
    } else if (statement.type === 'VariableDeclaration') {
      for (const decl of statement.declarations) {
        let init = decl.init;
        while (init && init.type === 'AssignmentExpression') init = init.left;
        if (!init || init.type !== 'MemberExpression') continue;
        if (init.object.name === 'exports') {
          definition[`${basename}.${init.property.name}`] =
            `${link}#L${statement.loc.start.line}`;
        } else if (init.object.name === 'module') {
          if (init.property.name !== 'exports') continue;
          exported.constructors.push(decl.id.name);
          definition[decl.id.name] = `${link}#L${statement.loc.start.line}`;
        }
      }
    }
  });

  // Scan for definitions matching those exports; currently supports:
  //
  //   ClassName.foo = ...;
  //   ClassName.prototype.foo = ...;
  //   function Identifier(...) {...};
  //   class Foo {...};
  //
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

      definition[name] = `${link}#L${statement.loc.start.line}`;

      if (expr.left.property.name === expr.right.name) {
        indirect[expr.right.name] = name;
      }

    } else if (statement.type === 'FunctionDeclaration') {
      const name = statement.id.name;
      if (!exported.identifiers.includes(name)) return;
      if (basename.startsWith('_')) return;
      definition[`${basename}.${name}`] =
        `${link}#L${statement.loc.start.line}`;

    } else if (statement.type === 'ClassDeclaration') {
      if (!exported.constructors.includes(statement.id.name)) return;
      definition[statement.id.name] = `${link}#L${statement.loc.start.line}`;

      const name = statement.id.name.slice(0, 1).toLowerCase() +
                  statement.id.name.slice(1);

      statement.body.body.forEach((defn) => {
        if (defn.type !== 'MethodDefinition') return;
        if (defn.kind === 'method') {
          definition[`${name}.${defn.key.name}`] =
            `${link}#L${defn.loc.start.line}`;
        } else if (defn.kind === 'constructor') {
          definition[`new ${statement.id.name}`] =
            `${link}#L${defn.loc.start.line}`;
        }
      });
    }
  });

  // Search for indirect references of the form ClassName.foo = foo;
  if (Object.keys(indirect).length > 0) {
    program.forEach((statement) => {
      if (statement.type === 'FunctionDeclaration') {
        const name = statement.id.name;
        if (indirect[name]) {
          definition[indirect[name]] = `${link}#L${statement.loc.start.line}`;
        }
      }
    });
  }
});

fs.writeFileSync(output, JSON.stringify(definition, null, 2), 'utf8');
