const { spawnSync } = require('child_process');
const env = Object.assign({}, process.env);
delete env.NODE_V8_COVERAGE
spawnSync(process.execPath, [require.resolve('./subprocess')], {
  env: env
});
