import { spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { describe, it } from 'node:test';
import { match, strictEqual } from 'node:assert';

describe('--experimental-default-type=module should not affect the interpretation of files with unknown extensions',
         { concurrency: true }, () => {
           it('should error on an entry point with an unknown extension', async () => {
             const { code, signal, stdout, stderr } = await spawnPromisified(process.execPath, [
               '--experimental-default-type=module',
               fixtures.path('es-modules/package-type-module/extension.unknown'),
             ]);

             match(stderr, /ERR_UNKNOWN_FILE_EXTENSION/);
             strictEqual(stdout, '');
             strictEqual(code, 1);
             strictEqual(signal, null);
           });

           it('should error on an import with an unknown extension', async () => {
             const { code, signal, stdout, stderr } = await spawnPromisified(process.execPath, [
               '--experimental-default-type=module',
               fixtures.path('es-modules/package-type-module/imports-unknownext.mjs'),
             ]);

             match(stderr, /ERR_UNKNOWN_FILE_EXTENSION/);
             strictEqual(stdout, '');
             strictEqual(code, 1);
             strictEqual(signal, null);
           });
         });
