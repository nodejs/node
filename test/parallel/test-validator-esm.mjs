import '../common/index.mjs';
import assert from 'node:assert';
import { suite, test } from 'node:test';

const { Schema, codes } = await import('node:validator');

suite('node:validator ESM support', () => {
  test('Schema is importable and functional', () => {
    const schema = new Schema({ type: 'string' });
    const result = schema.validate('hello');
    assert.strictEqual(result.valid, true);
    assert.strictEqual(result.errors.length, 0);
  });

  test('codes are accessible', () => {
    assert.strictEqual(codes.INVALID_TYPE, 'INVALID_TYPE');
  });

  test('validation errors work', () => {
    const schema = new Schema({ type: 'number', minimum: 10 });
    const result = schema.validate(5);
    assert.strictEqual(result.valid, false);
    assert.strictEqual(result.errors[0].code, codes.NUMBER_TOO_SMALL);
  });
});
