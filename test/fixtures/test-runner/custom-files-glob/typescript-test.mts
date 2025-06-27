import { test } from 'node:test';

// 'as string' ensures that type stripping actually occurs
test('typescript-test.mts should not run' as string);
