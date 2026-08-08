'use strict';
const path = require('path');
const { pathToFileURL } = require('url');
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  type: ['cjs', 'esm'],
  files: [1e2, 1e3],
  n: [10],
}, { flags: ['--experimental-vfs', '--no-warnings'] });

// Builds a module graph of `files` packages, each with its own package.json,
// an index requiring a package-local file and a shared root module, and an
// entry point that pulls in every package.
function buildGraph(layer, files, type) {
  const entryRequires = [];
  if (type === 'esm') {
    layer.writeFileSync('/package.json', '{"type":"module"}');
  }
  layer.writeFileSync('/shared.js',
                      type === 'cjs' ? 'module.exports = 0;' : 'export default 0;');
  for (let i = 0; i < files; i++) {
    layer.mkdirSync(`/${i}`, { recursive: true });
    if (type === 'cjs') {
      layer.writeFileSync(`/${i}/package.json`, '{"main":"index.js"}');
      layer.writeFileSync(`/${i}/lib.js`, 'module.exports = 1;');
      layer.writeFileSync(
        `/${i}/index.js`,
        'require("./lib.js"); require("../shared.js"); module.exports = __filename;');
      entryRequires.push(`require('./${i}/');`);
    } else {
      layer.writeFileSync(`/${i}/package.json`, '{"type":"module"}');
      layer.writeFileSync(`/${i}/lib.js`, 'export default 1;');
      layer.writeFileSync(
        `/${i}/index.js`,
        'import "./lib.js"; import "../shared.js"; export default import.meta.url;');
      entryRequires.push(`import './${i}/index.js';`);
    }
  }
  layer.writeFileSync('/entry.js', entryRequires.join('\n'));
}

async function main({ n, type, files }) {
  const vfs = require('node:vfs');
  const layer = vfs.create();
  buildGraph(layer, files, type);

  bench.start();
  for (let i = 0; i < n; i++) {
    const mountPoint = layer.mount();
    const entry = path.join(mountPoint, 'entry.js');
    if (type === 'cjs') {
      require(entry);
    } else {
      await import(pathToFileURL(entry).href);
    }
    // Unmounting purges the module caches for the mount prefix, so every
    // iteration is a cold load of the full graph.
    layer.unmount();
  }
  bench.end(n * files);
}
