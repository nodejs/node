'use strict';

require('../common');
const fixtures = require('../common/fixtures');
const errorModule = fixtures.path('async-error.js');
const { spawnSync } = require('child_process');

const main = `'use strict';

const four = require('${errorModule}');

async function main() {
  try {
    await four();
  } catch (e) {
    console.log(e);
  }
}

main();
`;

// --eval CJS
{
  const child = spawnSync(process.execPath, [
    '-e',
    main
  ], {
    env: { ...process.env }
  });

  if (child.status !== 0) {
    console.error(child.stderr.toString());
  }
  console.error(child.stdout.toString());
}
