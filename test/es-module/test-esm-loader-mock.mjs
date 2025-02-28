import '../common/index.mjs';
import assert from 'node:assert/strict';
import { mock } from '../fixtures/es-module-loaders/mock.mjs';

mock('node:events', {
  EventEmitter: 'This is mocked!'
});

// This resolves to node:events
// It is intercepted by mock-loader and doesn't return the normal value
assert.deepStrictEqual(await import('events'), Object.defineProperty({
  __proto__: null,
  EventEmitter: 'This is mocked!'
}, Symbol.toStringTag, {
  enumerable: false,
  value: 'Module'
}));

const mutator = mock('node:events', {
  EventEmitter: 'This is mocked v2!'
});

// It is intercepted by mock-loader and doesn't return the normal value.
// This is resolved separately from the import above since the specifiers
// are different.
const mockedV2 = await import('node:events');
assert.deepStrictEqual(mockedV2, Object.defineProperty({
  __proto__: null,
  EventEmitter: 'This is mocked v2!'
}, Symbol.toStringTag, {
  enumerable: false,
  value: 'Module'
}));

mutator.EventEmitter = 'This is mocked v3!';
assert.deepStrictEqual(mockedV2, Object.defineProperty({
  __proto__: null,
  EventEmitter: 'This is mocked v3!'
}, Symbol.toStringTag, {
  enumerable: false,
  value: 'Module'
}));
