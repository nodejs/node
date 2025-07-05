import { test } from 'node:test';

// 'as string' ensures that type stripping actually occurs
test('typescript-test.spec.mts this should pass' as string);
