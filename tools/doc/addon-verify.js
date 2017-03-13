'use strict';

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const marked = require('marked');

const rootDir = path.resolve(__dirname, '..', '..');
const doc = path.resolve(rootDir, 'doc', 'api', 'addons.md');
const verifyDir = path.resolve(rootDir, 'test', 'addons');

const contents = fs.readFileSync(doc).toString();

const tokens = marked.lexer(contents);
let files = null;
let id = 0;

// Just to make sure that all examples will be processed
tokens.push({ type: 'heading' });

for (var i = 0; i < tokens.length; i++) {
  var token = tokens[i];
  if (token.type === 'heading' && token.text) {
    if (files && Object.keys(files).length !== 0)
      verifyFiles(files, token.text);
    files = {};
  } else if (token.type === 'code') {
    var match = token.text.match(/^\/\/\s+(.*\.(?:cc|h|js))[\r\n]/);
    if (match === null)
      continue;
    files[match[1]] = token.text;
  }
}

function verifyFiles(files, blockName) {
  // must have a .cc and a .js to be a valid test
  if (!Object.keys(files).some((name) => /\.cc$/.test(name)) ||
      !Object.keys(files).some((name) => /\.js$/.test(name))) {
    return;
  }

  blockName = blockName
    .toLowerCase()
    .replace(/\s/g, '_')
    .replace(/[^a-z\d_]/g, '');
  const dir = path.resolve(
    verifyDir,
    `${(++id < 10 ? '0' : '') + id}_${blockName}`
  );

  files = Object.keys(files).map(function(name) {
    if (name === 'test.js') {
      files[name] = `'use strict';
const common = require('../../common');
${files[name].replace('Release', "' + common.buildType + '")}
`;
    }
    return {
      path: path.resolve(dir, name),
      name: name,
      content: files[name]
    };
  });

  files.push({
    path: path.resolve(dir, 'binding.gyp'),
    content: JSON.stringify({
      targets: [
        {
          target_name: 'addon',
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
    assert.strictEqual(e.code, 'EEXIST');
  }

  for (const file of files) {
    try {
      var content = fs.readFileSync(file.path);
    } catch (e) {
      assert.strictEqual(e.code, 'ENOENT');
    }

    // Only update when file content has changed to prevent unneeded rebuilds.
    if (content && content.toString() === file.content)
      continue;

    fs.writeFileSync(file.path, file.content);
    console.log('wrote', file.path);
  }
}
