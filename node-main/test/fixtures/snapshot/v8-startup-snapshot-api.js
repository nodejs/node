'use strict';

const fs = require('node:fs');
const zlib = require('node:zlib');
const path = require('node:path');
const assert = require('node:assert');

const v8 = require('node:v8');

class BookShelf {
  storage = new Map();

  // Reading a series of files from directory and store them into storage.
  constructor(directory, books) {
    for (const book of books) {
      this.storage.set(book, fs.readFileSync(path.join(directory, book)));
    };
  }

  static compressAll(shelf) {
    for (const [ book, content ] of shelf.storage) {
      shelf.storage.set(book, zlib.gzipSync(content));
    }
  }

  static decompressAll(shelf) {
    for (const [ book, content ] of shelf.storage) {
      shelf.storage.set(book, zlib.gunzipSync(content));
    }
  }
}

// __dirname here is where the snapshot script is placed
// during snapshot building time.
const shelf = new BookShelf(__dirname, [
  'book1.en_US.txt',
  'book1.es_ES.txt',
  'book2.zh_CN.txt',
]);

assert(v8.startupSnapshot.isBuildingSnapshot());

// On snapshot serialization, compress the books to reduce size.
v8.startupSnapshot.addSerializeCallback(BookShelf.compressAll, shelf);
// On snapshot deserialization, decompress the books.
v8.startupSnapshot.addDeserializeCallback(BookShelf.decompressAll, shelf);
v8.startupSnapshot.setDeserializeMainFunction((shelf) => {
  // process.env and process.argv are refreshed during snapshot
  // deserialization.
  const lang = process.env.BOOK_LANG || 'en_US';
  const book = process.argv[1];
  const name = `${book}.${lang}.txt`;
  console.error('Reading', name);
  console.log(shelf.storage.get(name).toString());
}, shelf);

assert.throws(() => v8.startupSnapshot.setDeserializeMainFunction(() => {
  assert.fail('unreachable duplicated main function');
}), {
  code: 'ERR_DUPLICATE_STARTUP_SNAPSHOT_MAIN_FUNCTION',
});
