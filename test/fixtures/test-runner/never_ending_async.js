const test = require('node:test');
const { setTimeout } = require('timers/promises');

test('never ending test', () => setTimeout(100_000_000));