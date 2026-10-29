const badMimeTypes = [
  null,  // no MIME type
  'text/plain',
  // JSON is only valid for JSON module imports, never for classic
  // importScripts(), even when the body is valid JavaScript.
  'application/json',
  'text/json',
  'application/manifest+json',
];

const validMimeTypes = [
  'application/ecmascript',
  'application/javascript',
  'application/x-ecmascript',
  'application/x-javascript',
  'text/ecmascript',
  'text/javascript',
  'text/javascript1.0',
  'text/javascript1.1',
  'text/javascript1.2',
  'text/javascript1.3',
  'text/javascript1.4',
  'text/javascript1.5',
  'text/jscript',
  'text/livescript',
  'text/x-ecmascript',
  'text/x-javascript',
];

function importScriptsWithMimeType(mimeType) {
  importScripts(`./mime-type-worker.py${mimeType ? '?mime=' + mimeType : ''}`);
}

importScripts('/resources/testharness.js');

for (const mimeType of badMimeTypes) {
  test(() => {
    assert_throws_dom(
      'NetworkError',
      () => { importScriptsWithMimeType(mimeType); },
      `importScripts with ${mimeType ? 'bad' : 'no'} MIME type ${mimeType || ''} throws NetworkError`,
    );
  }, `Importing script with ${mimeType ? 'bad' : 'no'} MIME type ${mimeType || ''}`);
}

for (const mimeType of validMimeTypes) {
  test(() => {
    try {
      importScriptsWithMimeType(mimeType);
    } catch {
      assert_unreached(`importScripts with MIME type ${mimeType} should not throw`);
    }
  }, `Importing script with valid JavaScript MIME type ${mimeType}`);
}
