'use strict';

// This test ensures that JavaScript file that includes
// a reserved Windows word can be loaded as ESM module

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const fs = require('fs').promises;
const path = require('path');

const imp = (file) => {
  return import(path.relative(__dirname, file).replace(/\\/g, '/'));
};

(async () => {
  tmpdir.refresh();
  const rel = (file) => path.join(tmpdir.path, file);

  { // Load a single script
    const file = rel('con.mjs');
    await fs.writeFile(file, 'export default "ok"');
    assert.strictEqual((await imp(file)).default, 'ok');
    await fs.unlink(file);
  }

  { // Load a module
    const entry = rel('entry.mjs');
    const nmDir = rel('node_modules');
    const mDir = rel('node_modules/con');
    const pkg = rel('node_modules/con/package.json');
    const script = rel('node_modules/con/index.mjs');

    await fs.writeFile(entry, 'export {default} from "con"');
    await fs.mkdir(nmDir);
    await fs.mkdir(mDir);
    await fs.writeFile(pkg, '{"main":"index.mjs"}');
    await fs.writeFile(script, 'export default "ok"');

    assert.strictEqual((await imp(entry)).default, 'ok');
    await fs.unlink(script);
    await fs.unlink(pkg);
    await fs.rmdir(mDir);
    await fs.rmdir(nmDir);
    await fs.unlink(entry);
  }
})().then(common.mustCall());
