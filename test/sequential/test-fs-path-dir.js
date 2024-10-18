'use strict';

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const path = require('path');
const fs = require('fs');
const { pathToFileURL } = require('url');

tmpdir.refresh();

let fileCounter = 1;
const nextFile = () => path.join(tmpdir.path, `file${fileCounter++}`);

const generateStringPath = (file, suffix = '') => file + suffix;

const generateURLPath = (file, suffix = '') =>
  pathToFileURL(file + suffix);

const generateUint8ArrayPath = (file, suffix = '') =>
  new Uint8Array(Buffer.from(file + suffix));

const generatePaths = (file, suffix = '') => [
  generateStringPath(file, suffix),
  generateURLPath(file, suffix),
  generateUint8ArrayPath(file, suffix),
];

const generateNewPaths = (suffix = '') => [
  generateStringPath(nextFile(), suffix),
  generateURLPath(nextFile(), suffix),
  generateUint8ArrayPath(nextFile(), suffix),
];

function checkSyncFn(syncFn, p, args, fail) {
  const result = fail ? 'must fail' : 'must succeed';
  const failMsg = `failed testing sync ${p} ${syncFn.name} ${result}`;
  if (!fail) {
    try {
      syncFn(p, ...args);
    } catch (e) {
      console.log(failMsg, e);
      throw e;
    }
  } else {
    assert.throws(() => syncFn(p, ...args), {
      code: 'ERR_FS_EISDIR',
    }, failMsg);
  }
}

function checkAsyncFn(asyncFn, p, args, fail) {
  const result = fail ? 'must fail' : 'must succeed';
  const failMsg = `failed testing async ${p} ${asyncFn.name} ${result}`;
  if (!fail) {
    try {
      asyncFn(p, ...args, common.mustCall());
    } catch (e) {
      console.log(failMsg, e);
      throw e;
    }
  } else {
    assert.throws(
      () => asyncFn(p, ...args, common.mustNotCall()), {
        code: 'ERR_FS_EISDIR',
      },
      failMsg
    );
  }
}

function checkPromiseFn(promiseFn, p, args, fail) {
  const result = fail ? 'must fail' : 'must succeed';
  const failMsg = `failed testing promise ${p} ${promiseFn.name} ${result}`;
  if (!fail) {
    const r = promiseFn(p, ...args).catch((err) => {
      console.log(failMsg, err);
      throw err;
    });
    r?.close?.();
  } else {
    assert.rejects(
      promiseFn(p, ...args), {
        code: 'ERR_FS_EISDIR',
      },
      failMsg
    ).then(common.mustCall());
  }
}

function check(asyncFn, syncFn, promiseFn,
               { suffix, fail, args = [] }) {
  const file = nextFile();
  fs.writeFile(file, 'data', (e) => {
    assert.ifError(e);
    const paths = generatePaths(file, suffix);
    for (const p of paths) {
      if (asyncFn) checkAsyncFn(asyncFn, p, args, fail);
      if (promiseFn) checkPromiseFn(promiseFn, p, args, fail);
      if (syncFn) checkSyncFn(syncFn, p, args, fail);
    }
  });
}

function checkManyToMany(asyncFn, syncFn, promiseFn,
                         { suffix, fail, args = [] }) {
  const source = nextFile();
  fs.writeFile(source, 'data', (err) => {
    assert.ifError(err);
    if (asyncFn) {
      generateNewPaths(suffix)
        .forEach((p) => checkAsyncFn(asyncFn, source, [p, ...args], fail));
    }
    if (promiseFn) {
      generateNewPaths(suffix)
        .forEach((p) => checkPromiseFn(promiseFn, source, [p, ...args], fail));
    }
    if (syncFn) {
      generateNewPaths(suffix)
        .forEach((p) => checkSyncFn(syncFn, source, [p, ...args], fail));
    }
  });
}
check(fs.readFile, fs.readFileSync, fs.promises.readFile,
      { suffix: '', fail: false });
check(fs.readFile, fs.readFileSync, fs.promises.readFile,
      { suffix: '/', fail: true });
check(fs.writeFile, fs.writeFileSync, fs.promises.writeFile,
      { suffix: '', fail: false, args: ['data'] });
check(fs.writeFile, fs.writeFileSync, fs.promises.writeFile,
      { suffix: '/', fail: true, args: ['data'] });
check(fs.appendFile, fs.appendFileSync, fs.promises.appendFile,
      { suffix: '', fail: false, args: ['data'] });
check(fs.appendFile, fs.appendFileSync, fs.promises.appendFile,
      { suffix: '/', fail: true, args: ['data'] });
check(undefined, fs.createReadStream, undefined,
      { suffix: '', fail: false });
check(undefined, fs.createReadStream, undefined,
      { suffix: '/', fail: true });
check(undefined, fs.createWriteStream, undefined,
      { suffix: '', fail: false });
check(undefined, fs.createWriteStream, undefined,
      { suffix: '/', fail: true });
check(fs.truncate, fs.truncateSync, fs.promises.truncate,
      { suffix: '', fail: false, args: [42] });
check(fs.truncate, fs.truncateSync, fs.promises.truncate,
      { suffix: '/', fail: true, args: [42] });

check(fs.open, fs.openSync, fs.promises.open, { suffix: '', fail: false });
check(fs.open, fs.openSync, fs.promises.open, { suffix: '/', fail: true });

checkManyToMany(fs.copyFile, fs.copyFileSync, fs.promises.copyFile,
                { suffix: '', fail: false });

checkManyToMany(fs.copyFile, fs.copyFileSync, fs.promises.copyFile,
                { suffix: '/', fail: true });

if (common.isWindows) {
  check(fs.readFile, fs.readFileSync, fs.promises.readFile,
        { suffix: '\\', fail: true });
  check(fs.writeFile, fs.writeFileSync, fs.promises.writeFile,
        { suffix: '\\', fail: true, args: ['data'] });
  check(fs.appendFile, fs.appendFileSync, fs.promises.appendFile,
        { suffix: '\\', fail: true, args: ['data'] });
  check(undefined, fs.createReadStream, undefined,
        { suffix: '\\', fail: true });
  check(undefined, fs.createWriteStream, undefined,
        { suffix: '\\', fail: true });
  check(fs.truncate, fs.truncateSync, fs.promises.truncate,
        { suffix: '\\', fail: true, args: [42] });
  check(fs.open, fs.openSync, fs.promises.open, { suffix: '\\', fail: true });
  checkManyToMany(fs.copyFile, fs.copyFileSync, fs.promises.copyFile,
                  { suffix: '\\', fail: true });
}
