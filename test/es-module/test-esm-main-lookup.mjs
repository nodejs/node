// Flags: --experimental-modules
import '../common';
import assert from 'assert';
import main from '../fixtures/es-modules/pjson-main';

assert.strictEqual(main, 'main');
