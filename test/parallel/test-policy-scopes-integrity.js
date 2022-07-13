'use strict';
// Flags: --expose-internals

const common = require('../common');

if (!common.hasCrypto) common.skip('missing crypto');
common.requireNoPackageJSONAbove();

const Manifest = require('internal/policy/manifest').Manifest;
const assert = require('assert');

// #region files
{
  const baseURLs = [
    // Localhost is special cased in spec
    'file://localhost/root',
    'file:///root',
    'file:///',
    'file:///root/dir1',
    'file:///root/dir1/',
    'file:///root/dir1/dir2',
    'file:///root/dir1/dir2/',
  ];

  {
    const manifest = new Manifest({
      scopes: {
        'file:///': {
          integrity: true
        }
      }
    });

    for (const href of baseURLs) {
      assert.strictEqual(
        manifest.assertIntegrity(href),
        true
      );
      assert.strictEqual(
        manifest.assertIntegrity(href, null),
        true
      );
      assert.strictEqual(
        manifest.assertIntegrity(href, ''),
        true
      );
    }
  }
  {
    const manifest = new Manifest({
      scopes: {
        'file:': {
          integrity: true
        }
      }
    });

    for (const href of baseURLs) {
      assert.strictEqual(
        manifest.assertIntegrity(href),
        true
      );
      assert.strictEqual(
        manifest.assertIntegrity(href, null),
        true
      );
      assert.strictEqual(
        manifest.assertIntegrity(href, ''),
        true
      );
    }
  }
  {
    const manifest = new Manifest({
      resources: {
        'file:///root/dir1/isolated': {},
        'file:///root/dir1/cascade': {
          cascade: true
        }
      },
      scopes: {
        'file:///root/dir1/': {
          integrity: true,
        },
        'file:///root/dir1/dir2/': {
          cascade: true,
        },
        'file:///root/dir1/censor/': {
        },
      }
    });
    assert.throws(
      () => {
        manifest.assertIntegrity('file:///root/dir1/isolated');
      },
      /ERR_MANIFEST_ASSERT_INTEGRITY/
    );
    assert.strictEqual(
      manifest.assertIntegrity('file:///root/dir1/cascade'),
      true
    );
    assert.strictEqual(
      manifest.assertIntegrity('file:///root/dir1/enoent'),
      true
    );
    assert.strictEqual(
      manifest.assertIntegrity('file:///root/dir1/dir2/enoent'),
      true
    );
    assert.throws(
      () => {
        manifest.assertIntegrity('file:///root/dir1/censor/enoent');
      },
      /ERR_MANIFEST_ASSERT_INTEGRITY/
    );
  }
}
// #endregion
// #region data
{
  const baseURLs = [
    'data:text/javascript,0',
    'data:text/javascript,0/1',
  ];

  {
    const manifest = new Manifest({
      scopes: {
        'data:text/': {
          integrity: true
        }
      }
    });

    for (const href of baseURLs) {
      assert.throws(
        () => {
          manifest.assertIntegrity(href);
        },
        /ERR_MANIFEST_ASSERT_INTEGRITY/
      );
    }
  }
  {
    const manifest = new Manifest({
      scopes: {
        'data:/': {
          integrity: true
        }
      }
    });

    for (const href of baseURLs) {
      assert.throws(
        () => {
          manifest.assertIntegrity(href);
        },
        /ERR_MANIFEST_ASSERT_INTEGRITY/
      );
    }
  }
  {
    const manifest = new Manifest({
      scopes: {
        'data:': {
          integrity: true
        }
      }
    });

    for (const href of baseURLs) {
      assert.strictEqual(manifest.assertIntegrity(href), true);
    }
  }
  {
    const manifest = new Manifest({
      scopes: {
        'data:text/javascript,0/': {
          integrity: true
        },
      }
    });

    for (const href of baseURLs) {
      assert.throws(
        () => {
          manifest.assertIntegrity(href);
        },
        /ERR_MANIFEST_ASSERT_INTEGRITY/
      );
    }
  }
}
// #endregion
// #region blob
{
  {
    const manifest = new Manifest({
      scopes: {
        'https://example.com/': {
          integrity: true
        }
      }
    });

    assert.strictEqual(
      manifest.assertIntegrity('blob:https://example.com/has-origin'),
      true
    );
  }
  {
    const manifest = new Manifest({
      scopes: {
      }
    });

    assert.throws(
      () => {
        manifest.assertIntegrity('blob:https://example.com/has-origin');
      },
      /ERR_MANIFEST_ASSERT_INTEGRITY/
    );
  }
  {
    const manifest = new Manifest({
      scopes: {
        'blob:https://example.com/has-origin': {
          cascade: true
        }
      }
    });

    assert.throws(
      () => {
        manifest.assertIntegrity('blob:https://example.com/has-origin');
      },
      /ERR_MANIFEST_ASSERT_INTEGRITY/
    );
  }
  {
    const manifest = new Manifest({
      resources: {
        'blob:https://example.com/has-origin': {
          cascade: true
        }
      },
      scopes: {
        'https://example.com': {
          integrity: true
        }
      }
    });

    assert.strictEqual(
      manifest.assertIntegrity('blob:https://example.com/has-origin'),
      true
    );
  }
  {
    const manifest = new Manifest({
      scopes: {
        'blob:': {
          integrity: true
        },
        'https://example.com': {
          cascade: true
        }
      }
    });

    assert.throws(
      () => {
        manifest.assertIntegrity('blob:https://example.com/has-origin');
      },
      /ERR_MANIFEST_ASSERT_INTEGRITY/
    );
    assert.strictEqual(
      manifest.assertIntegrity('blob:foo'),
      true
    );
  }
}
// #endregion
// #startonerror
{
  const manifest = new Manifest({
    scopes: {
      'file:///': {
        integrity: true
      }
    },
    onerror: 'throw'
  });
  assert.throws(
    () => {
      manifest.assertIntegrity('http://example.com');
    },
    /ERR_MANIFEST_ASSERT_INTEGRITY/
  );
}
{
  assert.throws(
    () => {
      new Manifest({
        scopes: {
          'file:///': {
            integrity: true
          }
        },
        onerror: 'unknown'
      });
    },
    /ERR_MANIFEST_UNKNOWN_ONERROR/
  );
}
// #endonerror
