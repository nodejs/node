'use strict';
require('../common');
const assert = require('assert');
const { ZipArchive } = require('zlib');
const fixtures = require('../common/fixtures');

const archive = fixtures.readSync('corepack.zip');

const zip = new ZipArchive(archive);
const toc = zip.getEntries();

assert.deepStrictEqual(toc, new Map([
  [ 'package/CHANGELOG.md', 0 ],
  [ 'package/LICENSE.md', 1 ],
  [ 'package/README.md', 2 ],
  [ 'package/dist/corepack.js', 3 ],
  [ 'package/dist/npm.js', 4 ],
  [ 'package/dist/npx.js', 5 ],
  [ 'package/dist/pnpm.js', 6 ],
  [ 'package/dist/pnpx.js', 7 ],
  [ 'package/dist/vcc.js', 8 ],
  // eslint-disable-next-line max-len
  [ 'package/dist/vendors-_yarn_berry_cache_proxy-agent-npm-5_0_0-41772f4b01-9_zip_node_modules_proxy-agent_index_js.js', 9 ],
  [ 'package/dist/yarn.js', 10 ],
  [ 'package/dist/yarnpkg.js', 11 ],
  [ 'package/package.json', 12 ],
  [ 'package/shims/corepack', 13 ],
  [ 'package/shims/corepack.cmd', 14 ],
  [ 'package/shims/corepack.ps1', 15 ],
  [ 'package/shims/nodewin/corepack', 16 ],
  [ 'package/shims/nodewin/corepack.cmd', 17 ],
  [ 'package/shims/nodewin/corepack.ps1', 18 ],
  [ 'package/shims/nodewin/npm', 19 ],
  [ 'package/shims/nodewin/npm.cmd', 20 ],
  [ 'package/shims/nodewin/npm.ps1', 21 ],
  [ 'package/shims/nodewin/npx', 22 ],
  [ 'package/shims/nodewin/npx.cmd', 23 ],
  [ 'package/shims/nodewin/npx.ps1', 24 ],
  [ 'package/shims/nodewin/pnpm', 25 ],
  [ 'package/shims/nodewin/pnpm.cmd', 26 ],
  [ 'package/shims/nodewin/pnpm.ps1', 27 ],
  [ 'package/shims/nodewin/pnpx', 28 ],
  [ 'package/shims/nodewin/pnpx.cmd', 29 ],
  [ 'package/shims/nodewin/pnpx.ps1', 30 ],
  [ 'package/shims/nodewin/yarn', 31 ],
  [ 'package/shims/nodewin/yarn.cmd', 32 ],
  [ 'package/shims/nodewin/yarn.ps1', 33 ],
  [ 'package/shims/nodewin/yarnpkg', 34 ],
  [ 'package/shims/nodewin/yarnpkg.cmd', 35 ],
  [ 'package/shims/nodewin/yarnpkg.ps1', 36 ],
  [ 'package/shims/npm', 37 ],
  [ 'package/shims/npm.cmd', 38 ],
  [ 'package/shims/npm.ps1', 39 ],
  [ 'package/shims/npx', 40 ],
  [ 'package/shims/npx.cmd', 41 ],
  [ 'package/shims/npx.ps1', 42 ],
  [ 'package/shims/pnpm', 43 ],
  [ 'package/shims/pnpm.cmd', 44 ],
  [ 'package/shims/pnpm.ps1', 45 ],
  [ 'package/shims/pnpx', 46 ],
  [ 'package/shims/pnpx.cmd', 47 ],
  [ 'package/shims/pnpx.ps1', 48 ],
  [ 'package/shims/yarn', 49 ],
  [ 'package/shims/yarn.cmd', 50 ],
  [ 'package/shims/yarn.ps1', 51 ],
  [ 'package/shims/yarnpkg', 52 ],
  [ 'package/shims/yarnpkg.cmd', 53 ],
  [ 'package/shims/yarnpkg.ps1', 54 ],
]));
