import '../common/index.mjs';
import { describe, it } from 'node:test';
import { setTimeout } from 'node:timers/promises';
import assert from 'node:assert';

describe('Mock Timers ESM Regression', () => {
    it('should work with node:timers/promises and runAll() in ESM', async (t) => {
        t.mock.timers.enable({ apis: ['Date', 'setTimeout'] });
        const startTime = Date.now();
        let t1 = 0;

        await Promise.all([
            (async () => {
                await setTimeout(1000);
                t1 = Date.now();
            })(),
            (async () => {
                // Wait for the next tick to ensure setTimeout has been called
                await new Promise((resolve) => process.nextTick(resolve));
                t.mock.timers.runAll();
            })(),
        ]);

        assert.strictEqual(t1 - startTime, 1000);
    });

    it('should work with node:timers/promises and tick() in ESM', async (t) => {
        t.mock.timers.enable({ apis: ['Date', 'setTimeout'] });
        const startTime = Date.now();
        let t1 = 0;

        await Promise.all([
            (async () => {
                await setTimeout(500);
                t1 = Date.now();
            })(),
            (async () => {
                await new Promise((resolve) => process.nextTick(resolve));
                t.mock.timers.tick(500);
            })(),
        ]);

        assert.strictEqual(t1 - startTime, 500);
    });
});
