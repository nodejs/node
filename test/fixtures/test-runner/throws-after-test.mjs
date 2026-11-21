import { test } from 'node:test';

test('registered before the error', () => {});
null.x;
test('never registered', () => {});
