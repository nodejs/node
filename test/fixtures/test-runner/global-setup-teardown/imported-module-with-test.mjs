import { test } from 'node:test';

test('Imported module Ok', () => {});
test('Imported module Fail', () => { throw new Error('fail'); });
