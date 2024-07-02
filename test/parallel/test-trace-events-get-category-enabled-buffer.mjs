// Flags: --expose-internals

import '../common/index.mjs';
import { it } from 'node:test';
import { createTracing, getEnabledCategories } from 'node:trace_events';
import assert from 'node:assert';

import binding from 'internal/test/binding';
const getCategoryEnabledBuffer = binding.internalBinding('trace_events').getCategoryEnabledBuffer;

it('should track enabled/disabled categories', () => {
  const random = Math.random().toString().slice(2);
  const category = `node.${random}`;

  const buffer = getCategoryEnabledBuffer(category);

  const tracing = createTracing({
    categories: [category],
  });

  assert.ok(buffer[0] === 0, `the buffer[0] should start with value 0, got: ${buffer[0]}`);

  tracing.enable();

  let currentCategories = getEnabledCategories();

  assert.ok(currentCategories.includes(category), `the getEnabledCategories should include ${category}, got: ${currentCategories}`);
  assert.ok(buffer[0] > 0, `the buffer[0] should be greater than 0, got: ${buffer[0]}`);

  tracing.disable();

  currentCategories = getEnabledCategories();
  assert.ok(currentCategories === undefined, `the getEnabledCategories should return undefined, got: ${currentCategories}`);
  assert.ok(buffer[0] === 0, `the buffer[0] should be 0, got: ${buffer[0]}`);
});
