// Flags: --experimental-vm-modules

'use strict';

const common = require('../common');
const { SourceTextModule, SyntheticModule } = require('vm');

const tm = new SourceTextModule('import "a"');

tm
  .link(async () => {
    const tm2 = new SourceTextModule('import "b"');
    tm2.link(async () => {
      const sm = new SyntheticModule([], () => {});
      await sm.link(() => {});
      await sm.evaluate();
      return sm;
    }).then(() => tm2.evaluate());
    return tm2;
  })
  .then(() => tm.evaluate())
  .then(common.mustCall())
  .catch(common.mustNotCall());
