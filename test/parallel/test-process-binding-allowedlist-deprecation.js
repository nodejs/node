'use strict';

const common = require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');

// Assert that all allowed process.binding modules are
// runtime deprecated.
const processBindingAllowList = [
  'async_wrap',
  'buffer',
  'cares_wrap',
  'config',
  'constants',
  'contextify',
  'crypto',
  'fs',
  'fs_event_wrap',
  'http_parser',
  'icu',
  'inspector',
  'js_stream',
  'natives',
  'os',
  'pipe_wrap',
  'process_wrap',
  'signal_wrap',
  'spawn_sync',
  'stream_wrap',
  'tcp_wrap',
  'tls_wrap',
  'tty_wrap',
  'udp_wrap',
  'url',
  'util',
  'uv',
  'v8',
  'zlib',
];

const requireCryptoModules = ['crypto', 'tls_wrap'];
const requireIntlModules = ['icu'];

for (const module of processBindingAllowList) {
  if (requireCryptoModules.includes(module) && !common.hasCrypto) {
    continue;
  }

  if (requireIntlModules.includes(module) && !common.hasIntl) {
    continue;
  }

  const { stderr } = spawnSync(
    process.execPath,
    [
      '-p', `process.binding('${module}');`,
    ]
  );
  assert.match(stderr.toString(), /process\.binding\(\) is deprecated/);
}
