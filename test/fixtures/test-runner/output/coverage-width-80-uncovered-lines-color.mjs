// Flags: --experimental-test-coverage
// here we can't import common module as the coverage will be different based on the system
// Unused imports are here in order to populate the coverage report
// eslint-disable-next-line no-unused-vars
import * as a from '../coverage-snap/b.js';
// eslint-disable-next-line no-unused-vars
import * as b from '../coverage-snap/a.js';
// eslint-disable-next-line no-unused-vars
import * as c from '../coverage-snap/many-uncovered-lines.js';

import { test } from 'node:test';

process.env.FORCE_COLOR = '3';
process.stdout.columns = 100;

test(`Coverage Print Fixed Width ${process.stdout.columns}`);
