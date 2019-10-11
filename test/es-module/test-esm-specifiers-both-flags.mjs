import { mustCall } from '../common/index.mjs';
import { exec } from 'child_process';
import assert from 'assert';

const expectedError =
  'cannot use --es-module-specifier-resolution ' +
  'and --experimental-specifier-resolution at the same time';

const flags = '--es-module-specifier-resolution=node ' +
              '--experimental-specifier-resolution=node';

exec(`${process.execPath} ${flags}`, mustCall((error) => {
  assert(error.message.includes(expectedError));
}));
