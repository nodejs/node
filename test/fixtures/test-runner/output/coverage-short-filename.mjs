// Flags: --experimental-test-coverage
// here we can't import common module as the coverage will be different based on the system
// Unused imports are here in order to populate the coverage report
// eslint-disable-next-line no-unused-vars
import * as a from '../coverage-snap/a.js';

import { test } from 'node:test';

test(`Coverage Print Short Filename`);
