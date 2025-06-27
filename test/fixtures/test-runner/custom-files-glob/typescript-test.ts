const test = require('node:test');

// 'as string' ensures that type stripping actually occurs
test('typescript-test.ts this should pass' as string);
