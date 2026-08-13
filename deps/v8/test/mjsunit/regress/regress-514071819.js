// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --async-stack-traces

const mainRealm = Realm.current();
const remoteRealm = Realm.create();

function checkStackTrace(allowedRealm, frames) {
  for (let i = 0; i < frames.length; i++) {
    let func = frames[i];
    let functionRealm = Realm.owner(func);
    assertEquals(allowedRealm, functionRealm,
        `Error: unexpected function Realm for function ${func.name}`);
  }
  return frames;
}

Realm.shared = {};

Realm.eval(remoteRealm, `
  Error.prepareStackTrace = (e, frames) => {
    // frames.map(f => { console.log("remote: ", f); });
    e.capturedStackTrace = frames.map(f => f.getFunction());
    return frames;
  };
  function remoteCaptureStack() {
    let err = new Error();
    // Trigger stack trace rendering.
    err.stack;
    return err.capturedStackTrace;
  }
  Realm.shared.remoteCaptureStack = remoteCaptureStack;
`);

Error.prepareStackTrace = (e, frames) => {
  // frames.map(f => { console.log("main: ", f); });
  e.capturedStackTrace = frames.map(f => f.getFunction());
  return frames;
};

function mainCaptureStack() {
  let err = new Error();
  // Trigger stack trace rendering.
  err.stack;
  return err.capturedStackTrace;
}
Realm.shared.mainCaptureStack = mainCaptureStack;

Realm.eval(remoteRealm, `
  async function remoteBaz(throwFromRemote) {
    if (throwFromRemote) {
      return Realm.shared.remoteCaptureStack();
    } else {
      return Realm.shared.mainCaptureStack();
    }
  }
  Realm.shared.remoteBaz = remoteBaz;
`);

async function bar(throwFromRemote) {
  await Promise.resolve();
  return Realm.shared.remoteBaz(throwFromRemote);
}

async function foo(throwFromRemote) {
  return await bar(throwFromRemote);
}

foo(true)
  .then((frames) => {
    // From the full call stack
    //   foo -> bar -> remoteBaz -> remoteCaptureStack
    // only
    //   ... -> remoteBaz -> remoteCaptureStack
    // frames belong to remote realm.
    assertEquals(2, frames.length);
    checkStackTrace(remoteRealm, frames);
  })
  .catch((e) => {
    console.error(e);
    quit(1);
  });

foo(false)
  .then((frames) => {
    // From the full call stack
    //   foo -> bar -> remoteBaz -> mainCaptureStack
    // only
    //   foo -> bar -> ... -> mainCaptureStack
    // frames belong to the main realm.
    assertEquals(3, frames.length);
    checkStackTrace(mainRealm, frames);
  })
  .catch((e) => {
    console.error(e);
    quit(1);
  });

Promise.all([foo(true)])
  .then(([frames]) => {
    // From the full call stack
    //   Promise.all -> foo -> bar -> remoteBaz -> remoteCaptureStack
    // only
    //   ... -> remoteBaz -> remoteCaptureStack
    // frames belong to the remote realm.
    assertEquals(2, frames.length);
    checkStackTrace(remoteRealm, frames);
  })
  .catch((e) => {
    console.error(e);
    quit(1);
  });

Promise.all([foo(false)])
  .then(([frames]) => {
    // From the full call stack
    //   Promise.all -> foo -> bar -> remoteBaz -> mainCaptureStack
    // only
    //   Promise.all -> foo -> bar -> ... -> mainCaptureStack
    // frames belong to the main realm.
    assertEquals(4, frames.length);
    checkStackTrace(mainRealm, frames);
  })
  .catch((e) => {
    console.error(e);
    quit(1);
  });
