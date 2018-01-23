'use strict';

const { strictEqual } = require('assert');
const fs = require('fs');
const path = require('path');
const marked = require('marked');

const rootDir = path.resolve(__dirname, '..', '..');
const doc = path.resolve(rootDir, 'doc', 'api', 'addons.md');
const verifyDir = path.resolve(rootDir, 'test', 'addons');

let id = 0;
let currentHeader;

const addons = {};
const content = fs.readFileSync(doc, 'utf8');
for (const { text, type } of marked.lexer(content)) {
  if (type === 'heading' && text) {
    currentHeader = text;
    addons[currentHeader] = {
      files: {}
    };
  }
  if (type === 'code') {
    const match = text.match(/^\/\/\s+(.*\.(?:cc|h|js))[\r\n]/);
    if (match !== null) {
      addons[currentHeader].files[match[1]] = text;
    }
  }
}

for (const header in addons) {
  let { files } = addons[header];

  // must have a .cc and a .js to be a valid test
  if (!Object.keys(files).some((name) => /\.cc$/.test(name)) ||
      !Object.keys(files).some((name) => /\.js$/.test(name))) {
    continue;
  }

  const blockName = header
    .toLowerCase()
    .replace(/\s/g, '_')
    .replace(/[^a-z\d_]/g, '');
  const dir = path.resolve(
    verifyDir,
    `${(++id < 10 ? '0' : '') + id}_${blockName}`
  );

  files = Object.entries(files).map(([name, content]) => {
    if (name === 'test.js') content = boilerplate(name, content);
    return { name, content, path: path.resolve(dir, name) };
  });

  files.push({
    path: path.resolve(dir, 'binding.gyp'),
    content: JSON.stringify({
      targets: [
        {
          target_name: 'binding',
          defines: [ 'V8_DEPRECATION_WARNINGS=1' ],
          sources: files.map(function(file) {
            return file.name;
          })
        }
      ]
    })
  });

  try {
    fs.mkdirSync(dir);
  } catch (e) {
    strictEqual(e.code, 'EEXIST');
  }

  for (const file of files) {
    let content;
    try {
      content = fs.readFileSync(file.path, 'utf8');
    } catch (e) {
      strictEqual(e.code, 'ENOENT');
    }

    // Only update when file content has changed to prevent unneeded rebuilds.
    if (content !== file.content) {
      fs.writeFileSync(file.path, file.content);
      console.log('wrote', file.path);
    }
  }
}

function boilerplate(name, content) {
  return `'use strict';
const common = require('../../common');
${content.replace(
    "'./build/Release/binding'",
    // eslint-disable-next-line no-template-curly-in-string
    '`./build/${common.buildType}/binding`')}
`;
}
