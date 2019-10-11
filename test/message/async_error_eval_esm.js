'use strict';

require('../common');
const { spawnSync } = require('child_process');

const four = require('../common/fixtures')
  .readSync('async-error.js')
  .toString()
  .split('\n')
  .slice(2, -2)
  .join('\n');

const main = `${four}

async function main() {
  try {
    await four();
  } catch (e) {
    console.log(e);
  }
}

main();
`;

// --eval ESM
{
  const child = spawnSync(process.execPath, [
    '--input-type',
    'module',
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
