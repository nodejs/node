'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const vfs = require('node:vfs');

// Test basic VFS watcher via vfs.watch()
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/file.txt', 'initial');

  const watcher = myVfs.watch('/file.txt', { interval: 50, persistent: false });
  assert.ok(watcher);
  assert.strictEqual(typeof watcher.on, 'function');
  assert.strictEqual(typeof watcher.close, 'function');

  watcher.on('change', common.mustCall((eventType, filename) => {
    assert.strictEqual(eventType, 'change');
    assert.strictEqual(filename, 'file.txt');
    watcher.close();
  }));

  // Trigger change after a small delay
  setTimeout(() => {
    myVfs.writeFileSync('/file.txt', 'updated');
  }, 100);
}

// Test VFS watcher detects file deletion (rename event)
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/delete-me.txt', 'content');

  const watcher = myVfs.watch('/delete-me.txt', { interval: 50, persistent: false });

  watcher.on('change', common.mustCall((eventType, filename) => {
    assert.strictEqual(eventType, 'rename');
    assert.strictEqual(filename, 'delete-me.txt');
    watcher.close();
  }));

  // Delete the file after a small delay
  setTimeout(() => {
    myVfs.unlinkSync('/delete-me.txt');
  }, 100);
}

// Test VFS watcher with listener passed directly
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/listener-test.txt', 'initial');

  const watcher = myVfs.watch(
    '/listener-test.txt',
    { interval: 50, persistent: false },
    common.mustCall((eventType, filename) => {
      assert.strictEqual(eventType, 'change');
      assert.strictEqual(filename, 'listener-test.txt');
      watcher.close();
    }),
  );

  setTimeout(() => {
    myVfs.writeFileSync('/listener-test.txt', 'updated');
  }, 100);
}

// Test VFS watchFile via vfs.watchFile()
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/watchfile.txt', 'initial');

  const statWatcher = myVfs.watchFile(
    '/watchfile.txt',
    { interval: 50, persistent: false },
    common.mustCall((curr, prev) => {
      assert.ok(curr);
      assert.ok(prev);
      assert.strictEqual(curr.isFile(), true);
      // Stats should have changed
      assert.notStrictEqual(curr.mtimeMs, prev.mtimeMs);
      myVfs.unwatchFile('/watchfile.txt');
    }),
  );

  assert.ok(statWatcher);

  setTimeout(() => {
    myVfs.writeFileSync('/watchfile.txt', 'updated content');
  }, 100);
}

// Test VFS unwatchFile removes listener
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/unwatch.txt', 'initial');

  const listener = common.mustNotCall();
  myVfs.watchFile('/unwatch.txt', { interval: 50, persistent: false }, listener);

  // Immediately unwatch
  myVfs.unwatchFile('/unwatch.txt', listener);

  // Change the file - listener should not be called
  setTimeout(() => {
    myVfs.writeFileSync('/unwatch.txt', 'updated');
  }, 100);
}

// Test watcher close() method
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/close-test.txt', 'content');

  const watcher = myVfs.watch('/close-test.txt', { interval: 50, persistent: false });

  watcher.on('close', common.mustCall());
  watcher.on('change', common.mustNotCall());

  // Close immediately
  watcher.close();

  // Change shouldn't trigger anything
  setTimeout(() => {
    myVfs.writeFileSync('/close-test.txt', 'updated');
  }, 100);
}

// Test fs.watch with mounted VFS
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/data/file.txt', 'initial');
  myVfs.mount('/virtual');

  const watcher = fs.watch('/virtual/data/file.txt', { interval: 50, persistent: false });

  watcher.on('change', common.mustCall((eventType, filename) => {
    assert.strictEqual(eventType, 'change');
    assert.strictEqual(filename, 'file.txt');
    watcher.close();
    myVfs.unmount();
  }));

  setTimeout(() => {
    // Use addFile to write directly to provider, bypassing mount path logic
    myVfs.addFile('/data/file.txt', 'updated via fs.watch');
  }, 100);
}

// Test fs.watchFile with mounted VFS
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/data/watchfile.txt', 'initial');
  myVfs.mount('/virtual2');

  const listener = common.mustCall((curr, prev) => {
    assert.ok(curr);
    assert.ok(prev);
    fs.unwatchFile('/virtual2/data/watchfile.txt', listener);
    myVfs.unmount();
  });

  fs.watchFile('/virtual2/data/watchfile.txt', { interval: 50, persistent: false }, listener);

  setTimeout(() => {
    // Use addFile to write directly to provider, bypassing mount path logic
    myVfs.addFile('/data/watchfile.txt', 'updated via fs.watchFile');
  }, 100);
}

// Test overlay mode - VFS file exists, should watch VFS
{
  const myVfs = vfs.create({ overlay: true });
  myVfs.writeFileSync('/file.txt', 'vfs content');
  myVfs.mount('/overlay-test');

  const watcher = fs.watch('/overlay-test/file.txt', { interval: 50, persistent: false });

  watcher.on('change', common.mustCall((eventType, filename) => {
    assert.strictEqual(eventType, 'change');
    assert.strictEqual(filename, 'file.txt');
    watcher.close();
    myVfs.unmount();
  }));

  setTimeout(() => {
    // Use addFile to write directly to provider, bypassing mount path logic
    myVfs.addFile('/file.txt', 'vfs updated');
  }, 100);
}

// Test watcher.unref() doesn't throw
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/unref-test.txt', 'content');

  const watcher = myVfs.watch('/unref-test.txt', { persistent: false });
  const result = watcher.unref();
  assert.strictEqual(result, watcher); // Should return this
  watcher.close();
}

// Test watcher.ref() doesn't throw
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/ref-test.txt', 'content');

  const watcher = myVfs.watch('/ref-test.txt', { persistent: false });
  watcher.unref();
  const result = watcher.ref();
  assert.strictEqual(result, watcher); // Should return this
  watcher.close();
}

// Test watching non-existent file starts with null stats
{
  const myVfs = vfs.create();

  const watcher = myVfs.watch('/nonexistent.txt', { interval: 50, persistent: false });

  watcher.on('change', common.mustCall((eventType, filename) => {
    // File was created
    assert.strictEqual(eventType, 'rename');
    assert.strictEqual(filename, 'nonexistent.txt');
    watcher.close();
  }));

  setTimeout(() => {
    myVfs.writeFileSync('/nonexistent.txt', 'now exists');
  }, 100);
}

// Test supportsWatch property
{
  const myVfs = vfs.create();
  assert.strictEqual(myVfs.provider.supportsWatch, true);
}

// Test multiple changes are detected
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/multi.txt', 'initial');

  let changeCount = 0;
  const watcher = myVfs.watch('/multi.txt', { interval: 30, persistent: false });

  // Keep process alive until test completes
  const keepAlive = setTimeout(() => {}, 500);

  watcher.on('change', common.mustCall(() => {
    changeCount++;
    if (changeCount >= 2) {
      clearTimeout(keepAlive);
      watcher.close();
    }
  }, 2));

  setTimeout(() => {
    myVfs.writeFileSync('/multi.txt', 'first update');
  }, 100);

  // Give enough time between changes for the poll to detect both
  setTimeout(() => {
    myVfs.writeFileSync('/multi.txt', 'second update');
  }, 300);
}

// Test fs.promises.watch with VFS
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/promises-test.txt', 'initial');
  myVfs.mount('/virtual-promises');

  const ac = new AbortController();

  (async () => {
    const watcher = fs.promises.watch('/virtual-promises/promises-test.txt', {
      signal: ac.signal,
      persistent: false,
    });

    // Schedule a change
    setTimeout(() => {
      myVfs.addFile('/promises-test.txt', 'updated');
    }, 100);

    // Schedule abort after getting one event
    let eventCount = 0;
    for await (const event of watcher) {
      assert.strictEqual(event.eventType, 'change');
      assert.strictEqual(event.filename, 'promises-test.txt');
      eventCount++;
      ac.abort();
    }
    assert.strictEqual(eventCount, 1);
    myVfs.unmount();
  })().then(common.mustCall());
}

// Test VFS promises.watch directly
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/direct-promises.txt', 'initial');

  const ac = new AbortController();

  (async () => {
    const watcher = myVfs.promises.watch('/direct-promises.txt', {
      signal: ac.signal,
      persistent: false,
    });

    // Schedule a change
    setTimeout(() => {
      myVfs.writeFileSync('/direct-promises.txt', 'updated');
    }, 100);

    let eventCount = 0;
    for await (const event of watcher) {
      assert.ok(event.eventType);
      assert.ok(event.filename);
      eventCount++;
      ac.abort();
    }
    assert.strictEqual(eventCount, 1);
  })().then(common.mustCall());
}

// Test recursive watching
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/parent/child', { recursive: true });
  myVfs.writeFileSync('/parent/file.txt', 'parent file');
  myVfs.writeFileSync('/parent/child/file.txt', 'child file');

  const watcher = myVfs.watch('/parent', { recursive: true, interval: 50, persistent: false });

  watcher.on('change', common.mustCall((eventType, filename) => {
    // Should detect change in subdirectory
    assert.strictEqual(eventType, 'change');
    // Filename should include relative path from watched dir
    assert.ok(filename.includes('child') || filename === 'file.txt');
    watcher.close();
  }));

  setTimeout(() => {
    myVfs.writeFileSync('/parent/child/file.txt', 'updated child');
  }, 100);
}

// Test recursive watching via fs.watch with mounted VFS
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/data/subdir', { recursive: true });
  myVfs.writeFileSync('/data/top.txt', 'top');
  myVfs.writeFileSync('/data/subdir/nested.txt', 'nested');
  myVfs.mount('/virtual-recursive');

  const watcher = fs.watch('/virtual-recursive/data', {
    recursive: true,
    interval: 50,
    persistent: false,
  });

  watcher.on('change', common.mustCall((eventType, filename) => {
    assert.strictEqual(eventType, 'change');
    watcher.close();
    myVfs.unmount();
  }));

  setTimeout(() => {
    myVfs.addFile('/data/subdir/nested.txt', 'updated nested');
  }, 100);
}
