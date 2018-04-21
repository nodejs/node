'use strict';

const { mkdir, readFileSync, writeFile } = require('fs');
const { resolve } = require('path');
const { lexer } = require('marked');

const rootDir = resolve(__dirname, '..', '..');
const doc = resolve(rootDir, 'doc', 'api', 'addons.md');
const verifyDir = resolve(rootDir, 'test', 'addons');

const tokens = lexer(readFileSync(doc, 'utf8'));
const addons = {};
let id = 0;
let currentHeader;

const validNames = /^\/\/\s+(.*\.(?:cc|h|js))[\r\n]/;
tokens.forEach(({ type, text }) => {
  if (type === 'heading') {
    currentHeader = text;
    addons[currentHeader] = { files: {} };
  }
  if (type === 'code') {
    const match = text.match(validNames);
    if (match !== null) {
      addons[currentHeader].files[match[1]] = text;
    }
  }
});

Object.keys(addons).forEach((header) => {
  verifyFiles(addons[header].files, header);
});

function verifyFiles(files, blockName) {
  const fileNames = Object.keys(files);

  // Must have a .cc and a .js to be a valid test.
  if (!fileNames.some((name) => name.endsWith('.cc')) ||
      !fileNames.some((name) => name.endsWith('.js'))) {
    return;
  }

  blockName = blockName.toLowerCase().replace(/\s/g, '_').replace(/\W/g, '');
  const dir = resolve(
    verifyDir,
    `${String(++id).padStart(2, '0')}_${blockName}`
  );

  files = fileNames.map((name) => {
    if (name === 'test.js') {
      files[name] = `'use strict';
const common = require('../../common');
${files[name].replace(
    "'./build/Release/addon'",
    // eslint-disable-next-line no-template-curly-in-string
    '`./build/${common.buildType}/addon`')}
`;
    }
    return {
      path: resolve(dir, name),
      name: name,
      content: files[name]
    };
  });

  files.push({
    path: resolve(dir, 'binding.gyp'),
    content: JSON.stringify({
      targets: [
        {
          target_name: 'addon',
          defines: [ 'V8_DEPRECATION_WARNINGS=1' ],
          sources: files.map(({ name }) => name)
        }
      ]
    })
  });

  mkdir(dir, () => {
    // Ignore errors.

    files.forEach(({ path, content }) => {
      writeFile(path, content, (err) => {
        if (err)
          throw err;

        console.log(`Wrote ${path}`);
      });
    });
  });
}
