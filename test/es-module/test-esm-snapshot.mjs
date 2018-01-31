// Flags: --experimental-modules
/* eslint-disable required-modules */
import '../common/index';
import './esm-snapshot-mutator';
import one from './esm-snapshot';
import assert from 'assert';

assert.strictEqual(one, 1);
