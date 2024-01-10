// Flags: --expose-internals
import '../common/index.mjs';
import assert from 'assert';

import eventSource from 'internal/event_source';

{
  const isValidLastEventId = eventSource.isValidLastEventId;

  assert.strictEqual(isValidLastEventId('valid'), true);
  assert.strictEqual(isValidLastEventId('in\u0000valid'), false);
  assert.strictEqual(isValidLastEventId('in\x00valid'), false);

  assert.strictEqual(isValidLastEventId(null), false);
  assert.strictEqual(isValidLastEventId(undefined), false);
  assert.strictEqual(isValidLastEventId(7), false);
}

{
  const isASCIINumber = eventSource.isASCIINumber;

  assert.strictEqual(isASCIINumber('123'), true);
  assert.strictEqual(isASCIINumber('123a'), false);
}
