// Flags: --no-warnings --expose-internals

'use strict';

const common = require('../common');

common.skipIfInspectorDisabled();

if (!common.hasCrypto) {
  common.skip('missing crypto');
}

const { hasOpenSSL3 } = require('../common/crypto');

if (!hasOpenSSL3) {
  common.skip('this test requires OpenSSL 3.x');
}

if (!common.hasIntl) {
  // A handful of the tests fail when ICU is not included.
  common.skip('missing Intl');
}

if (!common.hasQuic) {
  common.skip('this test requires QUIC');
}

if (process.config.variables.icu_small) {
  common.skip('this test assumes full ICU build');
}

if (process.config.variables.node_quic) {
  common.skip('this test assumes default configuration options');
}

const {
  generateConfigJsonSchema,
} = require('internal/options');
const schemaInDoc = require('../../doc/node-config-schema.json');
const assert = require('assert');

const schema = generateConfigJsonSchema();

// Ensures the published doc/node-config-schema.json stays in sync with the
// runtime schema produced from option metadata. Regenerate with:
//   node tools/gen_node_config_schema.mjs
assert.strictEqual(JSON.stringify(schema), JSON.stringify(schemaInDoc),
                   'doc/node-config-schema.json is out of date. ' +
                   'Run `node tools/gen_node_config_schema.mjs` to update it.');
