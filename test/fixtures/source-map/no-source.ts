function Throw() {
  throw new Error('foo');
}

Throw();

// To recreate:
//
// npx tsc --outDir test/fixtures/source-map --sourceMap test/fixtures/source-map/no-source.ts
// rename the "source.[0]" to "file-not-exists.ts"
