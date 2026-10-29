// Flags: --experimental-vfs
'use strict';

const common = require('../common');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const zlib = require('node:zlib');
const vfs = require('node:vfs');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();
const root = tmpdir.resolve('lower');
fs.mkdirSync(path.join(root, 'dir'), { recursive: true });
fs.writeFileSync(path.join(root, 'shared.txt'), 'lower');
fs.writeFileSync(path.join(root, 'dir', 'lower.txt'), 'from disk');
fs.writeFileSync(path.join(root, 'deleted.txt'), 'still on disk');
fs.mkdirSync(path.join(root, 'masked'));
fs.writeFileSync(path.join(root, 'masked', 'lower.txt'), 'lower');

const upper = new vfs.MemoryProvider();
const lower = new vfs.RealFSProvider(root);
const provider = new vfs.ComposableProvider([upper, lower]);
const view = vfs.create(provider, { emitExperimentalWarning: false });

assert.ok(provider instanceof vfs.VirtualProvider);
assert.deepStrictEqual(provider.providers, [upper, lower]);
assert.notStrictEqual(provider.providers, provider.providers);
assert.strictEqual(view.readFileSync('/shared.txt', 'utf8'), 'lower');
assert.strictEqual(view.readFileSync('/dir/lower.txt', 'utf8'), 'from disk');
assert.deepStrictEqual(view.readdirSync('/dir'), ['lower.txt']);

// Writes copy up; neither the file nor its parent on the lower layer changes.
view.appendFileSync('/shared.txt', '+upper');
assert.strictEqual(view.readFileSync('/shared.txt', 'utf8'), 'lower+upper');
assert.strictEqual(fs.readFileSync(path.join(root, 'shared.txt'), 'utf8'), 'lower');
view.writeFileSync('/dir/upper.txt', 'only in memory');
assert.deepStrictEqual(view.readdirSync('/dir').sort(), ['lower.txt', 'upper.txt']);
assert.strictEqual(provider.readdirSync('/dir', { withFileTypes: true })[0].parentPath,
                   '/dir');
assert.strictEqual(fs.existsSync(path.join(root, 'dir', 'upper.txt')), false);
assert.strictEqual(view.statSync('/dir/lower.txt').isFile(), true);

// Whiteouts hide lower files even after removing an upper shadow.
view.unlinkSync('/deleted.txt');
assert.strictEqual(view.existsSync('/deleted.txt'), false);
view.writeFileSync('/deleted.txt', 'recreated');
assert.strictEqual(view.readFileSync('/deleted.txt', 'utf8'), 'recreated');
assert.ok(view.readdirSync('/').includes('deleted.txt'));
view.unlinkSync('/deleted.txt');
assert.strictEqual(view.existsSync('/deleted.txt'), false);
assert.strictEqual(fs.readFileSync(path.join(root, 'deleted.txt'), 'utf8'), 'still on disk');

// A file in the upper layer must mask a directory in a lower layer.
upper.writeFileSync('/masked', 'shadow');
assert.strictEqual(view.readFileSync('/masked', 'utf8'), 'shadow');
assert.throws(() => view.readFileSync('/masked/lower.txt'), { code: 'ENOTDIR' });
upper.unlinkSync('/masked');

assert.throws(() => view.openSync('/shared.txt', 'wx'), { code: 'EEXIST' });
assert.throws(() => view.copyFileSync('/shared.txt', '/dir/lower.txt', fs.constants.COPYFILE_EXCL),
              { code: 'EEXIST' });

(async () => {
  assert.strictEqual(await view.promises.readFile('/dir/lower.txt', 'utf8'), 'from disk');
  await view.promises.writeFile('/dir/lower.txt', 'async shadow');
  assert.strictEqual(await view.promises.readFile('/dir/lower.txt', 'utf8'), 'async shadow');
  assert.strictEqual(fs.readFileSync(path.join(root, 'dir', 'lower.txt'), 'utf8'), 'from disk');
  assert.deepStrictEqual((await view.promises.readdir('/dir')).sort(), ['lower.txt', 'upper.txt']);
  view.renameSync('/dir/lower.txt', '/dir/renamed.txt');
  assert.strictEqual(view.existsSync('/dir/lower.txt'), false);
  assert.strictEqual(view.readFileSync('/dir/renamed.txt', 'utf8'), 'async shadow');
  assert.strictEqual(fs.existsSync(path.join(root, 'dir', 'lower.txt')), true);

  const chunks = [];
  const entries = [
    await zlib.ZipEntry.create('zip.txt', Buffer.from('from zip')),
    await zlib.ZipEntry.create('dir/archive.txt', Buffer.from('archived')),
  ];
  for await (const chunk of zlib.createZipArchive(entries)) chunks.push(chunk);
  const archive = new vfs.ZipProvider(new zlib.ZipBuffer(Buffer.concat(chunks)));
  const overlay = new vfs.MemoryProvider();
  const layered = vfs.create(new vfs.ComposableProvider([overlay, archive, lower]),
                             { emitExperimentalWarning: false });
  assert.strictEqual(layered.readFileSync('/zip.txt', 'utf8'), 'from zip');
  assert.deepStrictEqual(layered.readdirSync('/dir').sort(),
                         ['archive.txt', 'lower.txt']);
  layered.writeFileSync('/dir/archive.txt', 'in memory');
  assert.strictEqual(layered.readFileSync('/dir/archive.txt', 'utf8'), 'in memory');
  assert.strictEqual(archive.readFileSync('/dir/archive.txt', 'utf8'), 'archived');
  const fd = layered.openSync('/dir/lower.txt', fs.constants.O_WRONLY);
  layered.closeSync(fd);
  assert.strictEqual(layered.readFileSync('/dir/lower.txt', 'utf8'), 'from disk');
  layered.unlinkSync('/zip.txt');
  assert.strictEqual(layered.existsSync('/zip.txt'), false);
  assert.strictEqual(archive.readFileSync('/zip.txt', 'utf8'), 'from zip');
  assert.throws(() => layered.unlinkSync('/dir'), { code: 'EISDIR' });
  assert.deepStrictEqual(layered.readdirSync('/dir', { recursive: true }).sort(),
                         ['archive.txt', 'lower.txt']);
  const mount = layered.mount();
  try {
    assert.strictEqual(fs.readFileSync(path.join(mount, 'dir', 'archive.txt'), 'utf8'),
                       'in memory');
  } finally {
    layered.unmount();
  }
  const readonly = new vfs.MemoryProvider();
  readonly.setReadOnly();
  const readView = vfs.create(new vfs.ComposableProvider([readonly, lower]),
                              { emitExperimentalWarning: false });
  assert.strictEqual(readView.readFileSync('/shared.txt', 'utf8'), 'lower');
  assert.strictEqual(readView.readonly, true);
  assert.throws(() => readView.writeFileSync('/shared.txt', 'fail'), { code: 'EROFS' });
})().then(common.mustCall());

assert.throws(() => new vfs.ComposableProvider([]), { code: 'ERR_OUT_OF_RANGE' });
assert.throws(() => new vfs.ComposableProvider([{}]), { code: 'ERR_INVALID_ARG_TYPE' });
