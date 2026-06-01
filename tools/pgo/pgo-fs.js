'use strict';

// PGO Training Script: File System Operations
//
// Exercises the most common fs operations in Node.js applications:
// - readFile/writeFile (config loading, template rendering, static file serving)
// - stat/access (file existence checks, middleware, caching)
// - readdir (directory listings, file discovery, build tools)
// - read/write streams (log file appending, file upload/download)
// - mkdir/rmdir (temp directories, build artifacts)
// - path operations (resolve, join, parse — used constantly)
// - watch (file watchers in dev tools, though we only test creation here)
//
// This exercises: libuv filesystem operations, thread pool (async fs),
// Buffer allocation for file data, string encoding, path module.

const fs = require('fs');
const fsp = require('fs/promises');
const path = require('path');
const os = require('os');
const crypto = require('crypto');

const DURATION_MS = parseInt(process.env.PGO_TRAINING_DURATION, 10) || 15_000;

// Create a unique temp directory for our operations
const TEMP_DIR = path.join(
  os.tmpdir(),
  `node-pgo-fs-${process.pid}-${Date.now()}`,
);

// Test data
const MEDIUM_CONTENT = Array.from(
  { length: 100 },
  (_, i) => `Line ${i}: ${crypto.randomBytes(40).toString('hex')}`,
).join('\n');
const LARGE_CONTENT = crypto.randomBytes(256 * 1024).toString('base64');
const JSON_CONFIG = JSON.stringify(
  {
    database: {
      host: 'localhost',
      port: 5432,
      name: 'myapp',
      pool: { min: 2, max: 10 },
    },
    redis: { host: 'localhost', port: 6379, db: 0 },
    server: { port: 3000, host: '0.0.0.0', cors: { origin: '*' } },
    logging: { level: 'info', format: 'json', outputs: ['stdout', 'file'] },
    features: { darkMode: true, betaFeatures: false, maxUploadSize: 10485760 },
  },
  null,
  2,
);

function setup() {
  fs.mkdirSync(TEMP_DIR, { recursive: true });
  fs.mkdirSync(path.join(TEMP_DIR, 'subdir', 'nested'), { recursive: true });
  fs.mkdirSync(path.join(TEMP_DIR, 'logs'), { recursive: true });
  fs.mkdirSync(path.join(TEMP_DIR, 'uploads'), { recursive: true });
  fs.mkdirSync(path.join(TEMP_DIR, 'cache'), { recursive: true });

  // Pre-create files for reading
  for (let i = 0; i < 50; i++) {
    fs.writeFileSync(
      path.join(TEMP_DIR, `file-${i}.txt`),
      `Content of file ${i}\n`.repeat(10),
    );
  }
  for (let i = 0; i < 10; i++) {
    fs.writeFileSync(
      path.join(TEMP_DIR, 'subdir', `sub-${i}.json`),
      JSON.stringify({ id: i, data: `value-${i}` }),
    );
  }
  fs.writeFileSync(path.join(TEMP_DIR, 'config.json'), JSON_CONFIG);
  fs.writeFileSync(path.join(TEMP_DIR, 'large.dat'), LARGE_CONTENT);
  fs.writeFileSync(path.join(TEMP_DIR, 'medium.txt'), MEDIUM_CONTENT);
}

function cleanup() {
  try {
    fs.rmSync(TEMP_DIR, { recursive: true, force: true });
  } catch {
    // Best effort cleanup
  }
}

// Workload 1: readFile + writeFile (async, the dominant pattern)
async function workloadReadWriteAsync(iterations) {
  let ops = 0;

  for (let i = 0; i < iterations; i++) {
    // Read config file (very common — Express/Fastify load configs at startup)
    await fsp.readFile(path.join(TEMP_DIR, 'config.json'), 'utf8');
    ops++;

    // Read and parse JSON config
    const configStr = await fsp.readFile(
      path.join(TEMP_DIR, 'config.json'),
      'utf8',
    );
    JSON.parse(configStr);
    ops++;

    // Read binary file (static file serving)
    await fsp.readFile(path.join(TEMP_DIR, `file-${i % 50}.txt`));
    ops++;

    // Write new file (log rotation, temp files, uploads)
    const tempFile = path.join(TEMP_DIR, 'uploads', `upload-${i}.tmp`);
    await fsp.writeFile(
      tempFile,
      `Upload content ${i}: ${crypto.randomBytes(100).toString('hex')}`,
    );
    ops++;

    // Read back
    await fsp.readFile(tempFile, 'utf8');
    ops++;

    // Overwrite (atomic-ish update pattern)
    await fsp.writeFile(tempFile, `Updated content ${i}`);
    ops++;

    // Read large file (less frequently)
    if (i % 10 === 0) {
      await fsp.readFile(path.join(TEMP_DIR, 'large.dat'));
      ops++;
    }
  }
  return ops;
}

// Workload 2: readFileSync + writeFileSync (startup, require.resolve)
function workloadReadWriteSync(iterations) {
  let ops = 0;

  for (let i = 0; i < iterations; i++) {
    // Sync read (very common in require() for package.json, .json configs)
    fs.readFileSync(path.join(TEMP_DIR, 'config.json'), 'utf8');
    ops++;

    // Sync read various files
    fs.readFileSync(path.join(TEMP_DIR, `file-${i % 50}.txt`), 'utf8');
    ops++;

    // Sync JSON file read/parse (package.json pattern)
    for (let j = 0; j < 10; j++) {
      const content = fs.readFileSync(
        path.join(TEMP_DIR, 'subdir', `sub-${j}.json`),
        'utf8',
      );
      JSON.parse(content);
    }
    ops += 10;

    // Sync write (cache files, lock files)
    const cacheFile = path.join(TEMP_DIR, 'cache', `cache-${i % 20}.json`);
    fs.writeFileSync(
      cacheFile,
      JSON.stringify({ key: `key-${i}`, value: i * 42, ts: Date.now() }),
    );
    ops++;
  }
  return ops;
}

// Workload 3: stat / access / exists (file existence checks)
async function workloadStatAccess(iterations) {
  let ops = 0;

  for (let i = 0; i < iterations; i++) {
    // stat (used by express.static, require.resolve, etc.)
    await fsp.stat(path.join(TEMP_DIR, `file-${i % 50}.txt`));
    ops++;

    // stat directory
    await fsp.stat(path.join(TEMP_DIR, 'subdir'));
    ops++;

    // Access check (permission verification)
    try {
      await fsp.access(
        path.join(TEMP_DIR, `file-${i % 50}.txt`),
        fs.constants.R_OK,
      );
    } catch {
      /* expected for some */
    }
    ops++;

    // lstat (symlink-aware, used by tools)
    await fsp.lstat(path.join(TEMP_DIR, `file-${i % 50}.txt`));
    ops++;

    // Stat non-existent (cache miss pattern)
    try {
      await fsp.stat(path.join(TEMP_DIR, `nonexistent-${i}.txt`));
    } catch {
      /* expected */
    }
    ops++;

    // Sync variants (require.resolve uses these)
    fs.statSync(path.join(TEMP_DIR, `file-${i % 50}.txt`));
    ops++;

    fs.existsSync(path.join(TEMP_DIR, `file-${i % 50}.txt`));
    ops++;

    fs.existsSync(path.join(TEMP_DIR, `nonexistent-${i}.txt`));
    ops++;
  }
  return ops;
}

// Workload 4: readdir (directory listing, build tools, file watchers)
async function workloadReaddir(iterations) {
  let ops = 0;

  for (let i = 0; i < iterations; i++) {
    // Simple readdir
    await fsp.readdir(TEMP_DIR);
    ops++;

    // Readdir with file types (common for file tree traversal)
    await fsp.readdir(TEMP_DIR, { withFileTypes: true });
    ops++;

    // Readdir subdirectory
    await fsp.readdir(path.join(TEMP_DIR, 'subdir'), { withFileTypes: true });
    ops++;

    // Sync variant (build tools)
    fs.readdirSync(TEMP_DIR);
    ops++;

    // Recursive readdir (newer API, gaining adoption)
    if (i % 5 === 0) {
      await fsp.readdir(TEMP_DIR, { recursive: true });
      ops++;
    }
  }
  return ops;
}

// Workload 5: mkdir / rmdir / rename / copy (build tool patterns)
async function workloadDirectoryOps(iterations) {
  let ops = 0;

  for (let i = 0; i < iterations; i++) {
    const dir = path.join(TEMP_DIR, `build-${i}`);

    // mkdir (build output directories)
    await fsp.mkdir(dir, { recursive: true });
    ops++;

    // Create file in new dir
    const file = path.join(dir, 'output.txt');
    await fsp.writeFile(file, `Build output ${i}`);
    ops++;

    // Rename (atomic file update pattern)
    const newFile = path.join(dir, 'output.final.txt');
    await fsp.rename(file, newFile);
    ops++;

    // Copy file
    const copyDest = path.join(dir, 'output.backup.txt');
    await fsp.copyFile(newFile, copyDest);
    ops++;

    // rmdir (cleanup)
    await fsp.rm(dir, { recursive: true, force: true });
    ops++;
  }
  return ops;
}

// Workload 6: Stream read/write (large file processing, log files)
async function workloadStreams(iterations) {
  let ops = 0;

  for (let i = 0; i < iterations; i++) {
    // Read stream (file download, static serving of large files)
    await new Promise((resolve, reject) => {
      const rs = fs.createReadStream(path.join(TEMP_DIR, 'large.dat'), {
        highWaterMark: 16 * 1024,
      });
      rs.on('data', () => {});
      rs.on('end', () => {
        ops++;
        resolve();
      });
      rs.on('error', reject);
    });

    // Write stream (log file appending)
    await new Promise((resolve, reject) => {
      const ws = fs.createWriteStream(
        path.join(TEMP_DIR, 'logs', `app-${i % 5}.log`),
        { flags: 'a' },
      );
      for (let j = 0; j < 50; j++) {
        ws.write(
          `[${new Date().toISOString()}] INFO: Request processed in ${Math.random() * 100}ms\n`,
        );
      }
      ws.end(() => {
        ops++;
        resolve();
      });
      ws.on('error', reject);
    });

    // Pipe pattern (file copy via streams)
    if (i % 3 === 0) {
      await new Promise((resolve, reject) => {
        const rs = fs.createReadStream(path.join(TEMP_DIR, 'medium.txt'));
        const ws = fs.createWriteStream(
          path.join(TEMP_DIR, `pipe-copy-${i}.txt`),
        );
        rs.pipe(ws);
        ws.on('finish', () => {
          ops++;
          resolve();
        });
        ws.on('error', reject);
      });
    }
  }
  return ops;
}

// Workload 7: Path operations (used in every file operation)
function workloadPathOps(iterations) {
  let ops = 0;
  const testPaths = [
    '/usr/local/lib/node_modules/express/lib/router/index.js',
    'C:\\Users\\user\\project\\node_modules\\lodash\\lodash.js',
    '../relative/path/to/file.js',
    './src/components/Button/index.tsx',
    'https://example.com/path/to/resource?query=value#hash',
    '/path/with spaces/and (parens)/file name.txt',
  ];

  for (let i = 0; i < iterations; i++) {
    for (const p of testPaths) {
      path.resolve(p);
      path.dirname(p);
      path.basename(p);
      path.extname(p);
      path.parse(p);
      path.normalize(p);
      path.isAbsolute(p);
      ops += 7;
    }

    // path.join (the most common path operation)
    path.join('src', 'components', 'Button', 'index.tsx');
    path.join(TEMP_DIR, 'subdir', 'nested', 'file.txt');
    path.join('..', '..', 'node_modules', '.cache');
    ops += 3;

    // path.relative (monorepo/build tool pattern)
    path.relative(
      '/project/packages/core/src',
      '/project/packages/utils/src/helpers.js',
    );
    path.relative(TEMP_DIR, path.join(TEMP_DIR, 'subdir', 'file.txt'));
    ops += 2;

    // path.format (inverse of parse)
    path.format({
      root: '/',
      dir: '/home/user',
      base: 'file.txt',
      ext: '.txt',
      name: 'file',
    });
    ops++;
  }
  return ops;
}

async function main() {
  console.log('[pgo-fs] Starting file system workload...');

  setup();
  const startTime = Date.now();
  let totalOps = 0;
  let round = 0;
  const remaining = () => DURATION_MS - (Date.now() - startTime);

  try {
    while (remaining() > 0) {
      round++;
      const scale = Math.max(0.1, remaining() / DURATION_MS);
      const iterScale = (base) => Math.max(1, Math.floor(base * scale));

      // Path ops first — pure CPU, exercises V8 string handling
      if (round === 1) console.log('[pgo-fs] Running path operations...');
      totalOps += workloadPathOps(iterScale(2000));
      if (remaining() <= 0) break;

      // Sync reads (startup/require pattern)
      if (round === 1) console.log('[pgo-fs] Running sync read/write...');
      totalOps += workloadReadWriteSync(iterScale(100));
      if (remaining() <= 0) break;

      // Async reads/writes (most common runtime pattern)
      if (round === 1) console.log('[pgo-fs] Running async read/write...');
      totalOps += await workloadReadWriteAsync(iterScale(50));
      if (remaining() <= 0) break;

      // stat/access (middleware, require.resolve)
      if (round === 1) console.log('[pgo-fs] Running stat/access...');
      totalOps += await workloadStatAccess(iterScale(100));
      if (remaining() <= 0) break;

      // Directory operations
      if (round === 1) console.log('[pgo-fs] Running readdir...');
      totalOps += await workloadReaddir(iterScale(100));
      if (remaining() <= 0) break;

      if (round === 1) console.log('[pgo-fs] Running directory ops...');
      totalOps += await workloadDirectoryOps(iterScale(30));
      if (remaining() <= 0) break;

      // Stream operations
      if (round === 1) console.log('[pgo-fs] Running stream read/write...');
      totalOps += await workloadStreams(iterScale(20));
    }

    const elapsed = (Date.now() - startTime) / 1000;
    console.log(
      `[pgo-fs] Completed ${totalOps} fs operations in ${elapsed.toFixed(1)}s (${(totalOps / elapsed).toFixed(0)} ops/s) [${round} rounds]`,
    );
  } finally {
    cleanup();
  }
}

main().catch((err) => {
  console.error('[pgo-fs] Error:', err);
  cleanup();
  process.exit(1);
});
