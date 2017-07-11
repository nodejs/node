// Copyright AJ Jordan and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';

const assert = require('assert');
const path = require('path');
const remark = require('remark');
const man = require('remark-man');
const parser = remark().use(fixupHeader).use(synopsis).use(stability).use(description).use(man);
const dontBuild = ['_toc', 'all', 'addons', 'async_hooks', 'cli', 'deprecations', 'documentation', 'domain', 'errors', 'globals', 'http2', 'index', 'inspector', 'intl', 'n-api', 'process', 'punycode', 'synopsis', 'tracing', 'v8'];

module.exports = toMan;

function fixupHeader(opts) {
  return function _fixupHeader(ast, file, next) {
    const header = ast.children[0];

    assert.strictEqual(header.type, 'heading', 'First node is not a header');
    assert.strictEqual(header.depth, 1, 'Header is not of depth 1');

    const moduleName = header.children[0].value;
    header.children[0].value = `node-${moduleName.toLowerCase()}(3)`;
    header.children[0].value += ` -- Node.js ${moduleName} module`;

    // Attach the module name for other plugins in this file to use
    file.moduleName = moduleName;

    next();
  };
}

function synopsis(opts) {
  return function _synopsis(ast, file, next) {
    const moduleName = file.moduleName.toLowerCase();

    const heading = {
      type: 'heading',
      depth:  2,
      children: [
        {
          type: 'text',
          value: 'SYNOPSIS'
        }
      ]
    };

    const text = {
      type: 'paragraph',
      children: [
        {
          type: 'inlineCode',
          value: `const ${moduleName} = require('${moduleName}');`
        }
      ]
    };

    ast.children.splice(1, 0, heading);
    ast.children.splice(2, 0, text);

    next();
  };
}

function stability(opts) {
  return function _stability(ast, file, next) {
    const node = ast.children[4];

    assert.equal(node.type, 'blockquote', 'Stability information in unexpected location');

    const heading = {
      type: 'heading',
      depth:  2,
      children: [
        {
          type: 'text',
          value: 'STABILITY'
        }
      ]
    };

    ast.children.splice(3, 0, heading);
    // Unwrap the paragraph inside the blockquote
    ast.children[4] = node.children[0];

    next();
  };
}

function description(opts) {
  return function _description(ast, file, next) {
    const heading = {
      type: 'heading',
      depth:  2,
      children: [
        {
          type: 'text',
          value: 'DESCRIPTION'
        }
      ]
    };

    ast.children.splice(5, 0, heading);

    next();
  };
}
function toMan(input, inputFile, cb) {
  // Silently skip things we can't/shouldn't build
  if (dontBuild.includes(path.parse(inputFile).name)) return;

  parser.process(input, function(er, file) {
    cb(er, String(file));
  });
}
