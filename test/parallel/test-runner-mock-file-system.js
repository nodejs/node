'use strict';

require('../common');
const { test } = require('node:test');
const assert = require('assert');
const fs = require('fs');

// Helper to enable/disable the mock file system for isolation (async/await style)
async function withMockFS(test, options, fn) {
  const { mock } = test;
  mock.fileSystem.enable(options);
  try {
    await fn();
  } finally {
    mock.fileSystem.disable();
  }
}

test('fileSystem: read from a virtual file', async (t) => {
  await withMockFS(
    t,
    {
      files: { '/test.txt': 'Hello' },
      override: false,
    },
    async () => {
      assert.strictEqual(
        fs.readFileSync('/test.txt', 'utf8'),
        'Hello',
        'Mock file content should match'
      );
    }
  );
});

test('fileSystem: write to a virtual file', async (t) => {
  await withMockFS(
    t,
    {
      files: { '/test.txt': 'Hello' },
      override: false,
    },
    async () => {
      fs.writeFileSync('/new.txt', 'World');
      assert.strictEqual(
        fs.readFileSync('/new.txt', 'utf8'),
        'World',
        'Written mock file content should match'
      );
    }
  );
});

test('fileSystem: override mode reads from mock only', async (t) => {
  await withMockFS(
    t,
    {
      files: { '/mock.txt': 'Test' },
      override: true,
    },
    async () => {
      assert.strictEqual(
        fs.readFileSync('/mock.txt', 'utf8'),
        'Test',
        'Override mode should read mock file'
      );
    }
  );
});

test('fileSystem: override mode throws for non-mock files', async (t) => {
  await withMockFS(
    t,
    {
      files: { '/mock.txt': 'Test' },
      override: true,
    },
    async () => {
      assert.throws(
        () => fs.readFileSync('/real.txt'),
        /ENOENT/,
        'Non-mock file should throw in override mode'
      );
    }
  );
});

// Additional coverage using withMockFS

test('fileSystem: can re-enable with new files', async (t) => {
  await withMockFS(
    t,
    {
      files: { '/first.txt': 'One' },
      override: false,
    },
    async () => {
      assert.strictEqual(fs.readFileSync('/first.txt', 'utf8'), 'One');
    }
  );

  await withMockFS(
    t,
    {
      files: { '/second.txt': 'Two' },
      override: false,
    },
    async () => {
      assert.strictEqual(fs.readFileSync('/second.txt', 'utf8'), 'Two');
    }
  );
});

test('fileSystem: existsSync reflects mock files', async (t) => {
  await withMockFS(
    t,
    {
      files: { '/exists.txt': 'Present' },
      override: false,
    },
    async () => {
      assert.strictEqual(fs.existsSync('/exists.txt'), true);
      assert.strictEqual(fs.existsSync('/does-not-exist.txt'), false);
    }
  );
});

test('fileSystem: existsSync reflects override', async (t) => {
  await withMockFS(
    t,
    {
      files: { '/only.txt': 'Here' },
      override: true,
    },
    async () => {
      assert.strictEqual(fs.existsSync('/only.txt'), true);
      assert.strictEqual(fs.existsSync('/not-here.txt'), false);
    }
  );
});

test('fileSystem: disables and restores real fs', async (t) => {
  await withMockFS(
    t,
    {
      files: { '/dummy.txt': 'dummy' },
      override: true,
    },
    async () => {
      // nothing to check here
    }
  );
  // Should not throw for real file after disable
  assert.doesNotThrow(() => fs.readFileSync(__filename));
});

// Symbol.dispose support test (if implemented)
test('fileSystem: Symbol.dispose disables the mock', async (t) => {
  await withMockFS(
    t,
    {
      files: { '/dispose.txt': 'Bye' },
      override: false,
    },
    async () => {
      const { mock } = t;
      if (typeof mock.fileSystem[Symbol.dispose] === 'function') {
        mock.fileSystem[Symbol.dispose]();
        assert.throws(() => fs.readFileSync('/dispose.txt'), /not enabled/i);
      } else {
        mock.fileSystem.disable();
      }
    }
  );
});

// Edge case: using APIs when not enabled
test('fileSystem: throws if used when not enabled', async (t) => {
  const { mock } = t;
  mock.fileSystem.disable();
  assert.throws(() => fs.readFileSync('/x'), /not enabled/i);
  assert.throws(() => fs.writeFileSync('/x', 'y'), /not enabled/i);
  assert.throws(() => fs.existsSync('/x'), /not enabled/i);
});
