// Flags: --experimental-test-coverage
import * as a from '../coverage-snap/b.mjs';
import * as b from '../coverage-snap/a.mjs';
import { test } from 'node:test';

process.stdout.columns = 100;

test(`Coverage Print Fixed Width ${process.stdout.columns}`);