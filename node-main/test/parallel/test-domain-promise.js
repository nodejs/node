'use strict';
const common = require('../common');
const assert = require('assert');
const domain = require('domain');
const fs = require('fs');
const vm = require('vm');

process.on('warning', common.mustNotCall());

{
  const d = domain.create();

  d.run(common.mustCall(() => {
    Promise.resolve().then(common.mustCall(() => {
      assert.strictEqual(process.domain, d);
    }));
  }));
}

{
  const d = domain.create();

  d.run(common.mustCall(() => {
    Promise.resolve().then(() => {}).then(() => {}).then(common.mustCall(() => {
      assert.strictEqual(process.domain, d);
    }));
  }));
}

{
  const d = domain.create();

  d.run(common.mustCall(() => {
    vm.runInNewContext(`
      const promise = Promise.resolve();
      assert.strictEqual(promise.domain, undefined);
      promise.then(common.mustCall(() => {
        assert.strictEqual(process.domain, d);
      }));
    `, { common, assert, process, d });
  }));
}

{
  const d1 = domain.create();
  const d2 = domain.create();
  let p;
  d1.run(common.mustCall(() => {
    p = Promise.resolve(42);
  }));

  d2.run(common.mustCall(() => {
    p.then(common.mustCall((v) => {
      assert.strictEqual(process.domain, d2);
    }));
  }));
}

{
  const d1 = domain.create();
  const d2 = domain.create();
  let p;
  d1.run(common.mustCall(() => {
    p = Promise.resolve(42);
  }));

  d2.run(common.mustCall(() => {
    p.then(d1.bind(common.mustCall((v) => {
      assert.strictEqual(process.domain, d1);
    })));
  }));
}

{
  const d1 = domain.create();
  const d2 = domain.create();
  let p;
  d1.run(common.mustCall(() => {
    p = Promise.resolve(42);
  }));

  d1.run(common.mustCall(() => {
    d2.run(common.mustCall(() => {
      p.then(common.mustCall((v) => {
        assert.strictEqual(process.domain, d2);
      }));
    }));
  }));
}

{
  const d1 = domain.create();
  const d2 = domain.create();
  let p;
  d1.run(common.mustCall(() => {
    p = Promise.reject(new Error('foobar'));
  }));

  d2.run(common.mustCall(() => {
    p.catch(common.mustCall((v) => {
      assert.strictEqual(process.domain, d2);
    }));
  }));
}

{
  const d = domain.create();

  d.run(common.mustCall(() => {
    Promise.resolve().then(common.mustCall(() => {
      setTimeout(common.mustCall(() => {
        assert.strictEqual(process.domain, d);
      }), 0);
    }));
  }));
}

{
  const d = domain.create();

  d.run(common.mustCall(() => {
    Promise.resolve().then(common.mustCall(() => {
      fs.readFile(__filename, common.mustCall(() => {
        assert.strictEqual(process.domain, d);
      }));
    }));
  }));
}
{
  // Unhandled rejections become errors on the domain
  const d = domain.create();
  d.on('error', common.mustCall((e) => {
    assert.strictEqual(e.message, 'foo');
  }));
  d.run(common.mustCall(() => {
    Promise.reject(new Error('foo'));
  }));
}
