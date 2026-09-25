import { test } from 'node:test';
import { runShared } from './helper.mjs';
test('backup B', async (t) => { await runShared(t, 'B'); });
