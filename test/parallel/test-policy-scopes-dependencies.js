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
          dependencies: true
        }
      }
    });

    for (const href of baseURLs) {
      assert.strictEqual(
        manifest.getDependencyMapper(href).resolve('fs'),
        true
      );
    }
  }
  {
    const manifest = new Manifest({
      scopes: {
        '': {
          dependencies: true
        }
      }
    });

    for (const href of baseURLs) {
      assert.strictEqual(
        manifest.getDependencyMapper(href).resolve('fs'),
        true
      );
    }
  }
  {
    const manifest = new Manifest({
      scopes: {
        '': {
          dependencies: true
        },
        'file:': {
          cascade: true
        }
      }
    });

    for (const href of baseURLs) {
      assert.strictEqual(
        manifest.getDependencyMapper(href).resolve('fs'),
        true
      );
    }
  }
  {
    const manifest = new Manifest({
      scopes: {
        'file:': {
          dependencies: true
        }
      }
    });

    for (const href of baseURLs) {
      assert.strictEqual(
        manifest
          .getDependencyMapper(href)
          .resolve('fs'),
        true);
    }

    assert.strictEqual(
      manifest
        .getDependencyMapper('file://host/')
        .resolve('fs'),
      true);
  }
  {
    const manifest = new Manifest({
      resources: {
        'file:///root/dir1': {
          dependencies: {
            fs: 'test:fs1'
          }
        },
        'file:///root/dir1/isolated': {},
        'file:///root/dir1/cascade': {
          cascade: true
        }
      },
      scopes: {
        'file:///root/dir1/': {
          dependencies: {
            fs: 'test:fs2'
          }
        },
        'file:///root/dir1/censor/': {
        },
      }
    });

    for (const href of baseURLs) {
      const redirector = manifest.getDependencyMapper(href);
      if (href.startsWith('file:///root/dir1/')) {
        assert.strictEqual(
          redirector.resolve('fs').href,
          'test:fs2'
        );
      } else if (href === 'file:///root/dir1') {
        assert.strictEqual(
          redirector.resolve('fs').href,
          'test:fs1'
        );
      } else {
        assert.strictEqual(redirector.resolve('fs'), null);
      }
    }

    assert.strictEqual(
      manifest
        .getDependencyMapper('file:///root/dir1/isolated')
        .resolve('fs'),
      null
    );
    assert.strictEqual(
      manifest
        .getDependencyMapper('file:///root/dir1/cascade')
        .resolve('fs').href,
      'test:fs2'
    );
    assert.strictEqual(
      manifest
        .getDependencyMapper('file:///root/dir1/censor/foo')
        .resolve('fs'),
      null
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
          dependencies: {
            fs: true
          }
        }
      }
    });

    for (const href of baseURLs) {
      assert.strictEqual(
        manifest.getDependencyMapper(href).resolve('fs'),
        null);
    }
  }
  {
    const manifest = new Manifest({
      scopes: {
        'data:/': {
          dependencies: {
            fs: true
          }
        }
      }
    });

    for (const href of baseURLs) {
      assert.strictEqual(
        manifest.getDependencyMapper(href).resolve('fs'),
        null);
    }
  }
  {
    const manifest = new Manifest({
      scopes: {
        'data:': {
          dependencies: true
        }
      }
    });

    for (const href of baseURLs) {
      assert.strictEqual(
        manifest.getDependencyMapper(href).resolve('fs'),
        true
      );
    }
  }
  {
    const manifest = new Manifest({
      scopes: {
        'data:text/javascript,0/': {
          dependencies: {
            fs: 'test:fs1'
          }
        },
      }
    });

    for (const href of baseURLs) {
      assert.strictEqual(
        manifest.getDependencyMapper(href).resolve('fs'),
        null);
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
          dependencies: true
        }
      }
    });

    assert.strictEqual(
      manifest
          .getDependencyMapper('blob:https://example.com/has-origin')
          .resolve('fs'),
      true
    );
  }
  {
    const manifest = new Manifest({
      scopes: {
        'https://example.com': {
          dependencies: true
        }
      }
    });

    assert.strictEqual(
      manifest
          .getDependencyMapper('blob:https://example.com/has-origin')
          .resolve('fs'),
      true
    );
  }
  {
    const manifest = new Manifest({
      scopes: {
      }
    });

    assert.strictEqual(
      manifest
        .getDependencyMapper('blob:https://example.com/has-origin')
        .resolve('fs'),
      null);
  }
  {
    const manifest = new Manifest({
      scopes: {
        'blob:https://example.com/has-origin': {
          cascade: true
        }
      }
    });

    assert.strictEqual(
      manifest
        .getDependencyMapper('blob:https://example.com/has-origin')
        .resolve('fs'),
      null);
  }
  {
    const manifest = new Manifest({
      scopes: {
        // FIXME
        'https://example.com/': {
          dependencies: true
        },
        'blob:https://example.com/has-origin': {
          cascade: true
        }
      }
    });

    assert.strictEqual(
      manifest
        .getDependencyMapper('blob:https://example.com/has-origin')
        .resolve('fs'),
      true
    );
  }
  {
    const manifest = new Manifest({
      scopes: {
        'blob:': {
          dependencies: true
        },
        'blob:https://example.com/has-origin': {
          cascade: true
        }
      }
    });

    assert.strictEqual(
      manifest
        .getDependencyMapper('blob:https://example.com/has-origin')
        .resolve('fs'),
      null);
    assert.strictEqual(
      manifest
        .getDependencyMapper('blob:foo').resolve('fs'),
      true
    );
  }
}
// #endregion
