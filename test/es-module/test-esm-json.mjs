// Flags: --experimental-modules --experimental-json-modules
/* eslint-disable node-core/required-modules */

import '../common/index.mjs';
import { strictEqual } from 'assert';

import secret from '../fixtures/experimental.json';

strictEqual(secret.ofLife, 42);
