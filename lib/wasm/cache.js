'use strict';

const crypto = require('crypto');
const fs = require('fs');
const fsp = require('fs/promises');
const os = require('os');
const path = require('path');
const { setInterval, clearInterval } = require('timers');
const url = require('url');

const { Promise, PromiseAll } = primordials;

let cacheSubdir = '.cache/node/wasm-cache';
switch (os.platform()) {
  case 'darwin':
    cacheSubdir = 'Library/Caches/node/wasm-cache';
    break;
  case 'win32':
    cacheSubdir = 'AppData\\Local\\node\\wasm-cache';
    break;
}

let cacheDir = path.resolve(os.homedir(), cacheSubdir);

function getCachePath(modPath) {
  const h = crypto.createHash('sha256').update(modPath).digest('hex');
  return path.resolve(cacheDir, h);
}

async function statAndReadData(path) {
  const handle = await fsp.open(path, 'r');
  try {
    return await PromiseAll([handle.stat(), handle.readFile()]);
  } finally {
    handle.close();
  }
}

async function readBlobs(path) {
  const [[stat, data], [cacheStat, cacheData]] = await PromiseAll([
    statAndReadData(path),
    statAndReadData(getCachePath(path)).then((res) => res, () => [])
  ]);
  if (cacheStat && stat.mtimeMs < cacheStat.mtimeMs) {
    return { path, stat, data, cacheData };
  }
  return { path, stat, data };
}

function updateCache(blobs, data) {
  const cachePath = getCachePath(blobs.path);
  const cacheDir = path.dirname(cachePath);
  fs.mkdir(cacheDir, { recursive: true }, (err) => {
    fs.writeFile(getCachePath(blobs.path), data, () => {});
    // TODO
    blobs.onCacheValidated && blobs.onCacheValidated();
  });
}


internalBinding('wasm_streaming').setCallback(function(blobs, ws) {
  let isCacheValid = false;
  if (blobs.cacheData)
    isCacheValid = ws.setCompiledModuleBytes(blobs.cacheData);
  ws.setUrl(url.pathToFileURL(blobs.path).toString());
  ws.onBytesReceived(blobs.data);
  if (isCacheValid) {
    ws.finish();
    blobs.onCacheValidated && blobs.onCacheValidated();
  } else {
    ws.finish((data) => updateCache(blobs, data));
  }
});


// Promise<WebAssembly.Module> loadFile(path)
//
// Loads WebAssembly module from a local path.
// Maintains a cache internally so that subsequent compilations are
// almost instanteneous.
async function loadFile(path) {
  const blobs = await readBlobs(await fsp.realpath(path));
  return WebAssembly.compileStreaming(blobs);
}


// Promise precompileFile(path)
//
// Precompiles a WebAssembly module and inserts it into the cache.
// Normally loadFile() will update the cache transparently if needed.
// PrecompileFile() is handy when the cache doesn't persist (e.g. Node.js
// app shipped in a Docker container).
function precompileFile(path) {
  const res = new Promise((resolve, reject) => {
    fsp.realpath(path).then(readBlobs).then((blobs) => {
      blobs.onCacheValidated = function(err) {
        if (err) reject(err); else resolve();
      };
      return WebAssembly.compileStreaming(blobs);
    }).then(
      // Prevent GC from collecting the module:
      // Module is produced once Tier 1 compiler completes;
      // Tier 2 continues in the background, Tier 2 output is inserted
      // into the cache once available.  If Module is GC-ed, Tier 2 gets
      // cancelled.
      (mod) => { resolve.mod = mod; },
      reject
    );
  });
  // Prevent event loop from terminating early.
  const timerId = setInterval(() => {}, 24 * 60 * 60 * 1000 /* 1 day */);
  res.finally(() => clearInterval(timerId));
  return res;
}


// Gets directory to save compiled WASM modules.
function getCacheDir() {
  return cacheDir;
}


// Sets directory to save compiled WASM modules.
function setCacheDir(dir) {
  cacheDir = path.resolve(dir);
}

module.exports = {
  loadFile,
  precompileFile,
  getCacheDir,
  setCacheDir
};
