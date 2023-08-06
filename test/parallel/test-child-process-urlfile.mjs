import { isWindows, mustCall, mustSucceed, mustNotCall } from '../common/index.mjs';
import { strictEqual, throws } from 'node:assert';
import cp from 'node:child_process';
import url from 'node:url';
import tmpdir from '../common/tmpdir.js';

tmpdir.refresh();

const cwd = tmpdir.path;
const cwdUrl = url.pathToFileURL(cwd);
const whichCommand = isWindows ? 'where.exe cmd.exe' : 'which pwd';
const pwdFullPath = `${cp.execSync(whichCommand)}`.trim();
const pwdWHATWGUrl = url.pathToFileURL(pwdFullPath);

// Test for WHATWG URL instance, legacy URL, and URL-like object
for (const pwdUrl of [
  pwdWHATWGUrl,
  new url.URL(pwdWHATWGUrl),
  {
    href: pwdWHATWGUrl.href,
    origin: pwdWHATWGUrl.origin,
    protocol: pwdWHATWGUrl.protocol,
    username: pwdWHATWGUrl.username,
    password: pwdWHATWGUrl.password,
    host: pwdWHATWGUrl.host,
    hostname: pwdWHATWGUrl.hostname,
    port: pwdWHATWGUrl.port,
    pathname: pwdWHATWGUrl.pathname,
    search: pwdWHATWGUrl.search,
    searchParams: new URLSearchParams(pwdWHATWGUrl.searchParams),
    hash: pwdWHATWGUrl.hash,
  },
]) {
  const pwdCommandAndOptions = isWindows ?
    [pwdUrl, ['/d', '/c', 'cd'], { cwd: cwdUrl }] :
    [pwdUrl, [], { cwd: cwdUrl }];

  {
    cp.execFile(...pwdCommandAndOptions, mustSucceed((stdout) => {
      strictEqual(`${stdout}`.trim(), cwd);
    }));
  }

  {
    const stdout = cp.execFileSync(...pwdCommandAndOptions);
    strictEqual(`${stdout}`.trim(), cwd);
  }

  {
    const pwd = cp.spawn(...pwdCommandAndOptions);
    pwd.stdout.on('data', mustCall((stdout) => {
      strictEqual(`${stdout}`.trim(), cwd);
    }));
    pwd.stderr.on('data', mustNotCall());
    pwd.on('close', mustCall((code) => {
      strictEqual(code, 0);
    }));
  }

  {
    const stdout = cp.spawnSync(...pwdCommandAndOptions).stdout;
    strictEqual(`${stdout}`.trim(), cwd);
  }

}

// Test for non-URL objects
for (const badFile of [
  Buffer.from(pwdFullPath),
  {},
  { href: pwdWHATWGUrl.href },
  { pathname: pwdWHATWGUrl.pathname },
]) {
  const pwdCommandAndOptions = isWindows ?
    [badFile, ['/d', '/c', 'cd'], { cwd: cwdUrl }] :
    [badFile, [], { cwd: cwdUrl }];

  throws(
    () => cp.execFile(...pwdCommandAndOptions, mustNotCall()),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );

  throws(
    () => cp.execFileSync(...pwdCommandAndOptions),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );

  throws(
    () => cp.spawn(...pwdCommandAndOptions),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );

  throws(
    () => cp.spawnSync(...pwdCommandAndOptions),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
}
