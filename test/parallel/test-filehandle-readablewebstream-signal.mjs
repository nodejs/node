import { skip } from '../common/index.mjs';
import { open, access, constants } from 'node:fs/promises';
import assert from 'node:assert';

// Test that signal option in filehandle.readableWebStream() works

const shortFile = new URL(import.meta.url);

{
  await using fh = await open(shortFile);
  const cause = new Error('am abort reason');
  const controller = new AbortController();
  const { signal } = controller;
  controller.abort(cause);
  assert.throws(() => fh.readableWebStream({ signal, autoClose: false }), {
    code: 'ABORT_ERR',
    cause,
  });

  // Filehandle must be still open
  await fh.read();
}

{
  await using fh = await open(shortFile);
  const cause = new Error('am abort reason');
  const controller = new AbortController();
  const { signal } = controller;
  controller.abort(cause);
  assert.throws(() => fh.readableWebStream({ signal, autoClose: true }), {
    code: 'ABORT_ERR',
    cause,
  });

  // Filehandle must be closed after abort
  await assert.rejects(() => fh.read(), {
    code: 'EBADF',
  });
}

{
  await using fh = await open(shortFile);
  const cause = new Error('am abort reason');
  const controller = new AbortController();
  const { signal } = controller;
  const stream = fh.readableWebStream({ signal, autoClose: false });
  const reader = stream.getReader();
  controller.abort(cause);
  await assert.rejects(() => reader.read(), {
    code: 'ABORT_ERR',
    cause,
  });

  // Filehandle must be still open
  await fh.read();
}

{
  await using fh = await open(shortFile);
  const cause = new Error('am abort reason');
  const controller = new AbortController();
  const { signal } = controller;
  const stream = fh.readableWebStream({ signal, autoClose: true });
  const reader = stream.getReader();
  controller.abort(cause);
  await assert.rejects(() => reader.read(), {
    code: 'ABORT_ERR',
    cause,
  });

  // Filehandle must be closed after abort
  await assert.rejects(() => fh.read(), {
    code: 'EBADF',
  });
}

const longFile = new URL('file:///dev/zero');

try {
  await access(longFile, constants.R_OK);
} catch {
  skip('Can not perform long test');
}

{
  await using fh = await open(longFile);
  const cause = new Error('am abort reason');
  const controller = new AbortController();
  const { signal } = controller;
  const stream = fh.readableWebStream({ signal, autoClose: false });
  const reader = stream.getReader();
  setTimeout(() => controller.abort(cause), 100);
  await assert.rejects(async () => {
    while (true) {
      await new Promise((resolve) => setTimeout(resolve, 5));
      const { done } = await reader.read();
      assert.ok(done === false, 'we exhausted /dev/zero');
    }
  }, {
    code: 'ABORT_ERR',
    cause,
  });

  // Filehandle must be still open
  await fh.read();
}

{
  await using fh = await open(longFile);
  const cause = new Error('am abort reason');
  const controller = new AbortController();
  const { signal } = controller;
  const stream = fh.readableWebStream({ signal, autoClose: true });
  const reader = stream.getReader();
  setTimeout(() => controller.abort(cause), 100);
  await assert.rejects(async () => {
    while (true) {
      await new Promise((resolve) => setTimeout(resolve, 5));
      const { done } = await reader.read();
      assert.ok(done === false, 'we exhausted /dev/zero');
    }
  }, {
    code: 'ABORT_ERR',
    cause,
  });

  // Filehandle must be closed after abort
  await assert.rejects(() => fh.read(), {
    code: 'EBADF',
  });
}
