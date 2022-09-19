const { spawnSync } = require('child_process');
const env = { ...process.env, NODE_V8_COVERAGE: '' };
spawnSync(process.execPath, [require.resolve('./subprocess')], {
  env: env
});
