'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { spawnSyncAndAssert } = require('../common/child_process');
const tmpdir = require('../common/tmpdir');

const python = process.env.PYTHON || (common.isWindows ? 'python' : 'python3');
const rootDir = path.resolve(__dirname, '..', '..');
const toolsDir = path.join(rootDir, 'tools');
const configGypi = path.join(rootDir, 'config.gypi');

tmpdir.refresh();

// Mapping table matches deps/openssl/openssl_asm.gypi / openssl_no_asm.gypi.
spawnSyncAndAssert(python, ['-c', `
import sys
sys.path.insert(0, ${JSON.stringify(toolsDir)})
import install
assert install._OPENSSL_ARCHS[('x64', 'linux')] == 'linux-x86_64'
assert install._OPENSSL_ARCHS[('arm64', 'linux')] == 'linux-aarch64'
assert install._OPENSSL_ARCHS[('arm64', 'mac')] == 'darwin64-arm64-cc'
assert install._OPENSSL_ARCHS[('x64', 'win')] == 'VC-WIN64A'
assert install._OPENSSL_ARCHS[('arm64', 'win')] == 'VC-WIN64-ARM'
assert install._OPENSSL_ARCHS[('loong64', 'linux')] == 'linux64-loongarch64'
assert install._OPENSSL_ARCHS[('x64', 'openbsd')] == 'BSD-x86_64'
assert install._OPENSSL_ARCHS[('ia32', 'openbsd')] == 'BSD-x86'
assert install._OPENSSL_DISPATCHER_FALLBACK == 'linux-elf'
assert install._OPENSSL_MAC_ARCHS == ('darwin64-arm64-cc', 'darwin64-x86_64-cc')
old_platform = install.sys.platform
install.sys.platform = 'sunos5'
class _Opts:
    is_win = False
    variables = {'target_arch': 'x64'}
assert install.openssl_target_os(_Opts) == 'solaris'
assert install.openssl_wanted_archs(_Opts) == {'solaris64-x86_64-gcc', 'linux-elf'}
install.sys.platform = old_platform
print('ok')
`], { cwd: rootDir }, {
  stdout: (out) => assert.strictEqual(out.trim(), 'ok'),
});

// headers() only copies deps/openssl/config/archs when OpenSSL is bundled.
// --shared-openssl still has crypto, but those directories are not installed.
if (process.config.variables.node_shared_openssl)
  common.skip('shared OpenSSL does not install bundled arch headers');

function runHeadersInstall(dest, headersOnly) {
  const extra = headersOnly ? ', "--headers-only"' : '';
  spawnSyncAndAssert(python, ['-c', `
import sys
sys.path.insert(0, ${JSON.stringify(toolsDir)})
import install
options = install.parse_options([
    'install',
    '--dest-dir', ${JSON.stringify(dest)},
    '--prefix', '/',
    '--root-dir', ${JSON.stringify(rootDir)},
    '--config-gypi-path', ${JSON.stringify(configGypi)},
    '--silent'${extra},
])
install.headers(options, install.install)
print(install.openssl_arch_name(options) or '')
`], { cwd: rootDir }, {});
}

function installedArchs(dest) {
  const archsDir = path.join(dest, 'include', 'node', 'openssl', 'archs');
  return fs.readdirSync(archsDir).sort();
}

function sourceArchs() {
  return fs.readdirSync(path.join(rootDir, 'deps/openssl/config/archs')).sort();
}

const fullDest = tmpdir.resolve('full');
runHeadersInstall(fullDest, false);
const expectedArch = {
  win32: process.arch === 'arm64' ? 'VC-WIN64-ARM' : 'VC-WIN64A',
  darwin: process.arch === 'arm64' ? 'darwin64-arm64-cc' : 'darwin64-x86_64-cc',
  linux: {
    x64: 'linux-x86_64',
    arm64: 'linux-aarch64',
    arm: 'linux-armv4',
    ppc64: 'linux-ppc64le',
    s390x: 'linux64-s390x',
    riscv64: 'linux64-riscv64',
    loong64: 'linux64-loongarch64',
    mips64el: 'linux64-mips64',
  }[process.arch],
  freebsd: process.arch === 'x64' ? 'BSD-x86_64' : 'BSD-x86',
  sunos: process.arch === 'x64' ? 'solaris64-x86_64-gcc' : 'solaris-x86-gcc',
}[process.platform];

if (expectedArch) {
  const archs = installedArchs(fullDest);
  const expected = new Set([expectedArch, 'linux-elf']);
  if (process.platform === 'darwin') {
    expected.add('darwin64-arm64-cc');
    expected.add('darwin64-x86_64-cc');
  }
  assert.deepStrictEqual(
    archs,
    [...expected].sort(),
    `binary install should ship ${[...expected].sort().join(', ')} OpenSSL headers, got ${archs}`,
  );
}

const headersDest = tmpdir.resolve('headers-only');
runHeadersInstall(headersDest, true);
assert.deepStrictEqual(installedArchs(headersDest), sourceArchs());
