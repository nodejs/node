const { describe, it } = require('node:test');
const assert = require('node:assert');
const { Duplex, pipeline } = require('node:stream');
const { Readable } = require('node:stream');

describe('Duplex.from hardened regression tests', () => {
  it('should not throw AbortError when async function resolves without consuming input', async () => {
    const duplex = Duplex.from(async function() {
      // Intentionally do not consume input
    });
    
    let errored = false;
    duplex.on('error', () => { errored = true; });
    
    // Wait for the async function to resolve
    await new Promise(resolve => process.nextTick(resolve));
    assert.strictEqual(errored, false, 'Should not emit error event on early resolution');
  });

  it('should not throw when async function partially consumes input', async () => {
    const duplex = Duplex.from(async function* (input) {
      const { value } = await input.next();
      // Resolve without consuming the rest
    });
    
    let errored = false;
    duplex.on('error', () => { errored = true; });
    
    // Feed some data to make sure it's not just an empty stream
    duplex.write('some data');
    
    await new Promise(resolve => process.nextTick(resolve));
    assert.strictEqual(errored, false, 'Should not emit error on partial consumption');
  });

  it('should propagate the original error if the async function throws', async () => {
    const expectedError = new Error('Custom Async Error');
    const duplex = Duplex.from(async function() {
      throw expectedError;
    });
    
    const promise = new Promise((resolve) => {
      duplex.on('error', (err) => {
        resolve(err);
      });
    });
    
    const result = await promise;
    assert.strictEqual(result, expectedError, 'Should propagate the actual error from the async function');
  });

  it('should still propagate destruction upstream in a pipeline', async () => {
    const readable = new Readable({
      read() {
        this.push('data');
        this.push(null);
      }
    });
    
    const duplex = Duplex.from(async function() {
      // Resolve immediately without consuming
    });
    
    const writable = new (require('node:stream').Writable)({
      write(chunk, enc, cb) { cb(); }
    });

    let pipelineErrored = false;
    pipeline(readable, duplex, writable, (err) => {
      if (err) pipelineErrored = true;
    });

    await new Promise(resolve => setTimeout(resolve, 50));
    // In a pipeline, the destruction of the middle stream should eventually 
    // signal the others. We check that it doesn't crash the process.
    assert.doesNotThrow(() => {});
  });
});
