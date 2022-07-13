'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

(async function test() {
  const { strictEqual } = require('assert');
  const { Session } = require('inspector');
  const { promisify } = require('util');
  const vm = require('vm');
  const session = new Session();
  session.connect();
  session.post = promisify(session.post);
  await session.post('Debugger.enable');
  await check('http://example.com', 'http://example.com');
  await check(undefined, 'evalmachine.<anonymous>');
  await check('file:///foo.js', 'file:///foo.js');
  await check('file:///foo.js', 'file:///foo.js');
  await check('foo.js', 'foo.js');
  await check('[eval]', '[eval]');
  await check('%.js', '%.js');

  if (common.isWindows) {
    await check('C:\\foo.js', 'file:///C:/foo.js');
    await check('C:\\a\\b\\c\\foo.js', 'file:///C:/a/b/c/foo.js');
    await check('a:\\%.js', 'file:///a:/%25.js');
  } else {
    await check('/foo.js', 'file:///foo.js');
    await check('/a/b/c/d/foo.js', 'file:///a/b/c/d/foo.js');
    await check('/%%%.js', 'file:///%25%25%25.js');
  }

  async function check(filename, expected) {
    const promise =
      new Promise((resolve) => session.once('inspectorNotification', resolve));
    new vm.Script('42', { filename }).runInThisContext();
    const { params: { url } } = await promise;
    strictEqual(url, expected);
  }
})().then(common.mustCall());
