import { expectWarning } from '../common/index.mjs';
import assert from 'node:assert';
import { afterEach, beforeEach, describe, it, test } from 'node:test';

expectWarning('ExperimentalWarning', 'Test tags is an experimental feature and might change at any time');

test('child inherits parent suite tags', () => {
  describe('outer', { tags: ['db'] }, () => {
    it('child', (t) => {
      assert.deepStrictEqual(t.tags, ['db']);
    });
  });
});

test('child unions own tags with parent', () => {
  describe('outer', { tags: ['db'] }, () => {
    it('child', { tags: ['integration'] }, (t) => {
      assert.deepStrictEqual(t.tags, ['db', 'integration']);
    });
  });
});

test('parent tags appear before child tags (parent-first order)', () => {
  describe('outer', { tags: ['z'] }, () => {
    it('child', { tags: ['a'] }, (t) => {
      assert.deepStrictEqual(t.tags, ['z', 'a']);
    });
  });
});

test('union dedupes across parent and child', () => {
  describe('outer', { tags: ['db'] }, () => {
    it('child', { tags: ['DB', 'integration'] }, (t) => {
      assert.deepStrictEqual(t.tags, ['db', 'integration']);
    });
  });
});

test('multi-level nesting flattens by union', () => {
  describe('outer', { tags: ['a'] }, () => {
    describe('mid', { tags: ['b'] }, () => {
      it('leaf', { tags: ['c'] }, (t) => {
        assert.deepStrictEqual(t.tags, ['a', 'b', 'c']);
      });
    });
  });
});

test('child without own tags inherits parent set unchanged', () => {
  describe('outer', { tags: ['db', 'fast'] }, () => {
    it('child', (t) => {
      assert.deepStrictEqual(t.tags, ['db', 'fast']);
    });
  });
});

test('tagged child under untagged parent shows only own tags', () => {
  describe('outer', () => {
    it('child', { tags: ['x'] }, (t) => {
      assert.deepStrictEqual(t.tags, ['x']);
    });
  });
});

test('untagged child under untagged parent has empty tags', () => {
  describe('outer', () => {
    it('child', (t) => {
      assert.deepStrictEqual(t.tags, []);
    });
  });
});

test('tags: [] yields an empty array, not undefined', () => {
  it('empty', { tags: [] }, (t) => {
    assert.deepStrictEqual(t.tags, []);
  });
});

test('tags: [] on a child still inherits parent tags (not an opt-out)', () => {
  describe('outer', { tags: ['db'] }, () => {
    it('child', { tags: [] }, (t) => {
      assert.deepStrictEqual(t.tags, ['db']);
    });
  });
});

test('t.tags returns a frozen view', () => {
  it('frozen', { tags: ['db'] }, (t) => {
    const tags = t.tags;
    assert.strictEqual(Object.isFrozen(tags), true);
  });
});

test('successive t.tags reads return equivalent arrays', () => {
  it('idempotent', { tags: ['db', 'fast'] }, (t) => {
    assert.deepStrictEqual(t.tags, t.tags);
    assert.deepStrictEqual(t.tags, ['db', 'fast']);
  });
});

test('beforeEach and afterEach see the flattened tags of the current test', () => {
  describe('outer', { tags: ['db'] }, () => {
    beforeEach((t) => t.assert.deepStrictEqual(t.tags, ['db', 'integration']));
    afterEach((t) => t.assert.deepStrictEqual(t.tags, ['db', 'integration']));
    it('child', { tags: ['integration'] }, (t) => {
      t.assert.deepStrictEqual(t.tags, ['db', 'integration']);
    });
  });
});

test('context.test() accepts and inherits tags', async (t) => {
  await t.test('parent', { tags: ['db'] }, async (parent) => {
    parent.assert.deepStrictEqual(parent.tags, ['db']);
    await parent.test('child', { tags: ['integration'] }, (child) => {
      child.assert.deepStrictEqual(child.tags, ['db', 'integration']);
    });
  });
});
