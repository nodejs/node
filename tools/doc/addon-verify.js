'use strict';

// doc/api/addons.md has a bunch of code.  Extract it for verification
// that the C++ code compiles and the js code runs.
// Add .gyp files which will be used to compile the C++ code.
// Modify the require paths in the js code to pull from the build tree.
// Triggered from the build-addons target in the Makefile and vcbuild.bat.

const { mkdir, writeFile } = require('fs');
const { resolve } = require('path');
const vfile = require('to-vfile');
const unified = require('unified');
const remarkParse = require('remark-parse');

const rootDir = resolve(__dirname, '..', '..');
const doc = resolve(rootDir, 'doc', 'api', 'addons.md');
const verifyDir = resolve(rootDir, 'test', 'addons');

const file = vfile.readSync(doc, 'utf8');
const tree = unified().use(remarkParse).parse(file);
const addons = {};
let id = 0;
let currentHeader;

const validNames = /^\/\/\s+(.*\.(?:cc|h|js))[\r\n]/;
tree.children.forEach((node) => {
  if (node.type === 'heading') {
    currentHeader = file.contents.slice(
      node.children[0].position.start.offset,
      node.position.end.offset);
    addons[currentHeader] = { files: {} };
  } else if (node.type === 'code') {
    const match = node.value.match(validNames);
    if (match !== null) {
      addons[currentHeader].files[match[1]] = node.value;
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
          sources: files.map(({ name }) => name),
          includes: ['../common.gypi'],
        },
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
