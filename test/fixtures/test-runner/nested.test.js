'use strict';
const test = require('node:test');

test('level 1', async (t) => {
    await t.test('level 2.1', async (t) => {
        await t.test('level 3', () => {});
    });
    await t.test('level 2.2', async (t) => {
        await t.test('level 3', () => {});
    });
    await t.test('level 2.4', () => {});
});
