import '../common/index.mjs';
import { open } from 'node:fs/promises';
import { rejects } from 'node:assert';

{
  const fh = await open(new URL(import.meta.url));

  // TODO: remove autoClose option when it becomes default
  const readableStream = fh.readableWebStream({ autoClose: true });

  // Consume the stream
  await new Response(readableStream).text();

  // If reading the FileHandle after the stream is consumed fails,
  // then we assume the autoClose option worked as expected.
  await rejects(fh.read(), { code: 'EBADF' });
}

{
  await using fh = await open(new URL(import.meta.url));

  const readableStream = fh.readableWebStream({ autoClose: false });

  // Consume the stream
  await new Response(readableStream).text();

  // Filehandle must be still open
  await fh.read();
  await fh.close();
}
