import { test } from 'node:test';

test('mutates NODE_TEST_CONTEXT before writing', () => {
  // Attribution is decided once, while the runner-provided environment is still
  // intact. Assigning here must not turn it on for the write that follows.
  process.env.NODE_TEST_CONTEXT = 'child-v8';
  console.log('env-mutated-out-2d9c');
});
