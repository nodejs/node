'use strict';

// Test that objects created by the TextEncoderStream and TextDecoderStream APIs
// are created in the correct realm. The tests work by creating an iframe for
// each realm and then posting Javascript to them to be evaluated. Inputs and
// outputs are passed around via global variables in each realm's scope.

// Async setup is required before creating any tests, so require done() to be
// called.
setup({explicit_done: true});

function createRealm() {
  let iframe = document.createElement('iframe');
  const scriptEndTag = '<' + '/script>';
  iframe.srcdoc = `<!doctype html>
<script>
onmessage = event => {
  if (event.source !== window.parent) {
    throw new Error('unexpected message with source ' + event.source);
  }
  eval(event.data);
};
${scriptEndTag}`;
  iframe.style.display = 'none';
  document.body.appendChild(iframe);
  let realmPromiseResolve;
  const realmPromise = new Promise(resolve => {
    realmPromiseResolve = resolve;
  });
  iframe.onload = () => {
    realmPromiseResolve(iframe.contentWindow);
  };
  return realmPromise;
}

async function createRealms() {
  // All realms are visible on the global object so they can access each other.

  // The realm that the constructor function comes from.
  window.constructorRealm = await createRealm();

  // The realm in which the constructor object is called.
  window.constructedRealm = await createRealm();

  // The realm in which reading happens.
  window.readRealm = await createRealm();

  // The realm in which writing happens.
  window.writeRealm = await createRealm();

  // The realm that provides the definitions of Readable and Writable methods.
  window.methodRealm = await createRealm();

  await evalInRealmAndWait(methodRealm, `
  window.ReadableStreamDefaultReader =
      new ReadableStream().getReader().constructor;
  window.WritableStreamDefaultWriter =
      new WritableStream().getWriter().constructor;
`);
  window.readMethod = methodRealm.ReadableStreamDefaultReader.prototype.read;
  window.writeMethod = methodRealm.WritableStreamDefaultWriter.prototype.write;
}

// In order for values to be visible between realms, they need to be
// global. To prevent interference between tests, variable names are generated
// automatically.
const id = (() => {
  let nextId = 0;
  return () => {
    return `realmsId${nextId++}`;
  };
})();

// Eval string "code" in the content of realm "realm". Evaluation happens
// asynchronously, meaning it hasn't happened when the function returns.
function evalInRealm(realm, code) {
  realm.postMessage(code, window.origin);
}

// Same as evalInRealm() but returns a Promise which will resolve when the
// function has actually.
async function evalInRealmAndWait(realm, code) {
  const resolve = id();
  const waitOn = new Promise(r => {
    realm[resolve] = r;
  });
  evalInRealm(realm, code);
  evalInRealm(realm, `${resolve}();`);
  await waitOn;
}

// The same as evalInRealmAndWait but returns the result of evaluating "code" as
// an expression.
async function evalInRealmAndReturn(realm, code) {
  const myId = id();
  await evalInRealmAndWait(realm, `window.${myId} = ${code};`);
  return realm[myId];
}

// Constructs an object in constructedRealm and copies it into readRealm and
// writeRealm. Returns the id that can be used to access the object in those
// realms. |what| can contain constructor arguments.
async function constructAndStore(what) {
  const objId = id();
  // Call |constructorRealm|'s constructor from inside |constructedRealm|.
  writeRealm[objId] = await evalInRealmAndReturn(
      constructedRealm, `new parent.constructorRealm.${what}`);
  readRealm[objId] = writeRealm[objId];
  return objId;
}

// Calls read() on the readable side of the TransformStream stored in
// readRealm[objId]. Locks the readable side as a side-effect.
function readInReadRealm(objId) {
  return evalInRealmAndReturn(readRealm, `
parent.readMethod.call(window.${objId}.readable.getReader())`);
}

// Calls write() on the writable side of the TransformStream stored in
// writeRealm[objId], passing |value|. Locks the writable side as a
// side-effect.
function writeInWriteRealm(objId, value) {
  const valueId = id();
  writeRealm[valueId] = value;
  return evalInRealmAndReturn(writeRealm, `
parent.writeMethod.call(window.${objId}.writable.getWriter(),
                        window.${valueId})`);
}

window.onload = () => {
  createRealms().then(() => {
    runGenericTests('TextEncoderStream');
    runTextEncoderStreamTests();
    runGenericTests('TextDecoderStream');
    runTextDecoderStreamTests();
    done();
  });
};

function runGenericTests(classname) {
  promise_test(async () => {
    const obj = await evalInRealmAndReturn(
        constructedRealm, `new parent.constructorRealm.${classname}()`);
    assert_equals(obj.constructor, constructorRealm[classname],
                  'obj should be in constructor realm');
  }, `a ${classname} object should be associated with the realm the ` +
     'constructor came from');

  promise_test(async () => {
    const objId = await constructAndStore(classname);
    const readableGetterId = id();
    readRealm[readableGetterId] = Object.getOwnPropertyDescriptor(
        methodRealm[classname].prototype, 'readable').get;
    const writableGetterId = id();
    writeRealm[writableGetterId] = Object.getOwnPropertyDescriptor(
        methodRealm[classname].prototype, 'writable').get;
    const readable = await evalInRealmAndReturn(
        readRealm, `${readableGetterId}.call(${objId})`);
    const writable = await evalInRealmAndReturn(
        writeRealm, `${writableGetterId}.call(${objId})`);
    assert_equals(readable.constructor, constructorRealm.ReadableStream,
                  'readable should be in constructor realm');
    assert_equals(writable.constructor, constructorRealm.WritableStream,
                  'writable should be in constructor realm');
  }, `${classname}'s readable and writable attributes should come from the ` +
     'same realm as the constructor definition');
}

function runTextEncoderStreamTests() {
  promise_test(async () => {
    const objId = await constructAndStore('TextEncoderStream');
    const writePromise = writeInWriteRealm(objId, 'A');
    const result = await readInReadRealm(objId);
    await writePromise;
    assert_equals(result.constructor, constructorRealm.Object,
                  'result should be in constructor realm');
    assert_equals(result.value.constructor, constructorRealm.Uint8Array,
                  'chunk should be in constructor realm');
  }, 'the output chunks when read is called after write should come from the ' +
     'same realm as the constructor of TextEncoderStream');

  promise_test(async () => {
    const objId = await constructAndStore('TextEncoderStream');
    const chunkPromise = readInReadRealm(objId);
    writeInWriteRealm(objId, 'A');
    // Now the read() should resolve.
    const result = await chunkPromise;
    assert_equals(result.constructor, constructorRealm.Object,
                  'result should be in constructor realm');
    assert_equals(result.value.constructor, constructorRealm.Uint8Array,
                  'chunk should be in constructor realm');
  }, 'the output chunks when write is called with a pending read should come ' +
     'from the same realm as the constructor of TextEncoderStream');

  // There is not absolute consensus regarding what realm exceptions should be
  // created in. Implementations may vary. The expectations in exception-related
  // tests may change in future once consensus is reached.
  promise_test(async t => {
    const objId = await constructAndStore('TextEncoderStream');
    // Read first to relieve backpressure.
    const readPromise = readInReadRealm(objId);
    // promise_rejects() does not permit directly inspecting the rejection, so
    // it's necessary to write it out long-hand.
    let writeSucceeded = false;
    try {
      // Write an invalid chunk.
      await writeInWriteRealm(objId, {
        toString() { return {}; }
      });
      writeSucceeded = true;
    } catch (err) {
      assert_equals(err.constructor, constructorRealm.TypeError,
                    'write TypeError should come from constructor realm');
    }
    assert_false(writeSucceeded, 'write should fail');

    let readSucceeded = false;
    try {
      await readPromise;
      readSucceeded = true;
    } catch (err) {
      assert_equals(err.constructor, constructorRealm.TypeError,
                    'read TypeError should come from constructor realm');
    }

    assert_false(readSucceeded, 'read should fail');
  }, 'TypeError for unconvertable chunk should come from constructor realm ' +
     'of TextEncoderStream');
}

function runTextDecoderStreamTests() {
  promise_test(async () => {
    const objId = await constructAndStore('TextDecoderStream');
    const writePromise = writeInWriteRealm(objId, new Uint8Array([65]));
    const result = await readInReadRealm(objId);
    await writePromise;
    assert_equals(result.constructor, constructorRealm.Object,
                  'result should be in constructor realm');
    // A string is not an object, so doesn't have an associated realm. Accessing
    // string properties will create a transient object wrapper belonging to the
    // current realm. So checking the realm of result.value is not useful.
  }, 'the result object when read is called after write should come from the ' +
     'same realm as the constructor of TextDecoderStream');

  promise_test(async () => {
    const objId = await constructAndStore('TextDecoderStream');
    const chunkPromise = readInReadRealm(objId);
    writeInWriteRealm(objId, new Uint8Array([65]));
    // Now the read() should resolve.
    const result = await chunkPromise;
    assert_equals(result.constructor, constructorRealm.Object,
                  'result should be in constructor realm');
    // A string is not an object, so doesn't have an associated realm. Accessing
    // string properties will create a transient object wrapper belonging to the
    // current realm. So checking the realm of result.value is not useful.
  }, 'the result object when write is called with a pending ' +
     'read should come from the same realm as the constructor of TextDecoderStream');

  promise_test(async t => {
    const objId = await constructAndStore('TextDecoderStream');
    // Read first to relieve backpressure.
    const readPromise = readInReadRealm(objId);
    // promise_rejects() does not permit directly inspecting the rejection, so
    // it's necessary to write it out long-hand.
    let writeSucceeded = false;
    try {
      // Write an invalid chunk.
      await writeInWriteRealm(objId, {});
      writeSucceeded = true;
    } catch (err) {
      assert_equals(err.constructor, constructorRealm.TypeError,
                    'write TypeError should come from constructor realm');
    }
    assert_false(writeSucceeded, 'write should fail');

    let readSucceeded = false;
    try {
      await readPromise;
      readSucceeded = true;
    } catch (err) {
      assert_equals(err.constructor, constructorRealm.TypeError,
                    'read TypeError should come from constructor realm');
    }
    assert_false(readSucceeded, 'read should fail');
  }, 'TypeError for chunk with the wrong type should come from constructor ' +
     'realm of TextDecoderStream');

  promise_test(async t => {
    const objId =
          await constructAndStore(`TextDecoderStream('utf-8', {fatal: true})`);
    // Read first to relieve backpressure.
    const readPromise = readInReadRealm(objId);
    // promise_rejects() does not permit directly inspecting the rejection, so
    // it's necessary to write it out long-hand.
    let writeSucceeded = false;
    try {
      await writeInWriteRealm(objId, new Uint8Array([0xff]));
      writeSucceeded = true;
    } catch (err) {
      assert_equals(err.constructor, constructorRealm.TypeError,
                    'write TypeError should come from constructor realm');
    }
    assert_false(writeSucceeded, 'write should fail');

    let readSucceeded = false;
    try {
      await readPromise;
      readSucceeded = true;
    } catch (err) {
      assert_equals(err.constructor, constructorRealm.TypeError,
                    'read TypeError should come from constructor realm');
    }
    assert_false(readSucceeded, 'read should fail');
  }, 'TypeError for invalid chunk should come from constructor realm ' +
     'of TextDecoderStream');

  promise_test(async t => {
    const objId =
          await constructAndStore(`TextDecoderStream('utf-8', {fatal: true})`);
    // Read first to relieve backpressure.
    readInReadRealm(objId);
    // Write an unfinished sequence of bytes.
    const incompleteBytesId = id();
    writeRealm[incompleteBytesId] = new Uint8Array([0xf0]);
    // promise_rejects() does not permit directly inspecting the rejection, so
    // it's necessary to write it out long-hand.
    let closeSucceeded = false;
    try {
      // Can't use writeInWriteRealm() here because it doesn't make it possible
      // to reuse the writer.
      await evalInRealmAndReturn(writeRealm, `
(() => {
  const writer = window.${objId}.writable.getWriter();
  parent.writeMethod.call(writer, window.${incompleteBytesId});
  return parent.methodRealm.WritableStreamDefaultWriter.prototype
    .close.call(writer);
})();
`);
      closeSucceeded = true;
    } catch (err) {
      assert_equals(err.constructor, constructorRealm.TypeError,
                    'close TypeError should come from constructor realm');
    }
    assert_false(closeSucceeded, 'close should fail');
  }, 'TypeError for incomplete input should come from constructor realm ' +
     'of TextDecoderStream');
}
