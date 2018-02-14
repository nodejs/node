// Flags: --experimental-modules
/* eslint-disable required-modules */
import assert from 'assert';
import main from '../fixtures/es-modules/pjson-main';

assert.strictEqual(main, 'main');
