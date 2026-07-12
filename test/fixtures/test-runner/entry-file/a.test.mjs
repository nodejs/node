import { test } from 'node:test';
import { runShared } from './helper.mjs';
test('backup A', async (t) => { await runShared(t, 'A'); });
