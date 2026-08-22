'use strict';
// Flags: --expose-internals

const common = require('../common');
const { it } = require('node:test');

try {
  require('trace_events');
} catch {
  common.skip('missing trace events');
}

common.skipIfPerfettoEnabled();

const { createTracing, getEnabledCategories } = require('trace_events');
const assert = require('assert');

const { categoryEnabledChecker } = require('internal/trace_events');

it('should track enabled/disabled categories', () => {
  const random = Math.random().toString().slice(2);
  const category = `node.${random}`;

  const isEnabled = categoryEnabledChecker(category);

  const tracing = createTracing({
    categories: [category],
  });

  assert.strictEqual(isEnabled(), false);

  tracing.enable();

  let currentCategories = getEnabledCategories();

  assert.ok(currentCategories.includes(category), `the getEnabledCategories should include ${category}, got: ${currentCategories}`);
  assert.strictEqual(isEnabled(), true);

  tracing.disable();

  currentCategories = getEnabledCategories();
  assert.ok(currentCategories === undefined, `the getEnabledCategories should return undefined, got: ${currentCategories}`);
  assert.strictEqual(isEnabled(), false);
});
