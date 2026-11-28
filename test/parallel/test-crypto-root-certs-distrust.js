'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

// src/node_root_certs_distrust.h is generated from tools/certdata.txt by
// tools/mk-root-certs-distrust.mjs. This test regenerates it into a temporary
// directory and checks that the committed header is up to date, so that a
// root-certificate update that brings a new CKA_NSS_SERVER_DISTRUST_AFTER
// date cannot land without the matching table entry.

const assert = require('assert');
const { spawnSync } = require('child_process');
const fs = require('fs');
const path = require('path');
const tmpdir = require('../common/tmpdir');

const root = path.resolve(__dirname, '..', '..');
const script = path.join(root, 'tools', 'mk-root-certs-distrust.mjs');
const committed = path.join(root, 'src', 'node_root_certs_distrust.h');
if (!fs.existsSync(script) || !fs.existsSync(committed))
  common.skip('source tree not available');

tmpdir.refresh();
const generated = path.join(tmpdir.path, 'node_root_certs_distrust.h');

const { status, stderr } = spawnSync(process.execPath, [
  script, '--output', generated,
], { encoding: 'utf8' });
assert.strictEqual(status, 0, stderr);

assert.strictEqual(
  fs.readFileSync(generated, 'utf8'),
  fs.readFileSync(committed, 'utf8'),
  'src/node_root_certs_distrust.h is out of date; ' +
  'run tools/mk-root-certs-distrust.mjs');
