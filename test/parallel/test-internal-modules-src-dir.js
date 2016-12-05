'use strict';
const common = require('../common');
const child_process = require('child_process');
const assert = require('assert');
const path = require('path');

const fakeDirs = path.join(common.fixturesDir,
                           'fake-internal-module-source-dir');

const with_zlib_dir = path.join(fakeDirs, 'with-zlib');
const bootstrap_node = path.join(fakeDirs, 'with-bootstrap_node');

assert.strictEqual(
    'foobar\n',
    child_process.execSync(`${process.execPath} ` +
                           `--internal-modules-source-dir=${with_zlib_dir} ` +
                           '-p zlib',
                           { encoding: 'utf8' }));

assert.strictEqual(
    42,
    child_process.spawnSync(process.execPath,
                            [`--internal-modules-source-dir=${bootstrap_node}`,
                             '-p', '0']).status);

const entry = path.join(bootstrap_node, 'internal/bootstrap_node.js');

assert.strictEqual(
    42,
    child_process.spawnSync(process.execPath,
                            [`--js-entry-point=${entry}`,
                             '-p', '0']).status);
