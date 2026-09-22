// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-os-system

class CustomError extends Error {}
const throwingObj = {
  toString() { throw new CustomError(); }
};

// D8 core file and execution operations
assertThrows(() => read(throwingObj), CustomError);
assertThrows(() => read("nonexistent", throwingObj), CustomError);
assertThrows(() => readbuffer(throwingObj), CustomError);
assertThrows(() => load(throwingObj), CustomError);
assertThrows(() => write(throwingObj, "test"), CustomError);
assertThrows(() => writeFile(throwingObj, "test"), CustomError);
assertThrows(() => writeFile(throwingObj, new Uint8Array([1, 2, 3])), CustomError);

// D8 file object
if (typeof d8 !== 'undefined' && d8.file) {
  assertThrows(() => d8.file.exists(throwingObj), CustomError);
}

// OS builtins
if (typeof os !== 'undefined') {
  if (os.chdir) {
    assertThrows(() => os.chdir(throwingObj), CustomError);
  }
  if (os.mkdirp) {
    assertThrows(() => os.mkdirp(throwingObj), CustomError);
  }
  if (os.rmdir) {
    assertThrows(() => os.rmdir(throwingObj), CustomError);
  }
  if (os.setenv) {
    assertThrows(() => os.setenv(throwingObj, "value"), CustomError);
    assertThrows(() => os.setenv("key", throwingObj), CustomError);
  }
  if (os.unsetenv) {
    assertThrows(() => os.unsetenv(throwingObj), CustomError);
  }
  if (os.system) {
    assertThrows(() => os.system(throwingObj), CustomError);
    assertThrows(() => os.system("echo", [throwingObj]), CustomError);
    let throwingArray = [];
    Object.defineProperty(throwingArray, 0, {
      get() { throw new CustomError(); }
    });
    throwingArray.length = 1;
    assertThrows(() => os.system("echo", throwingArray), CustomError);
  }
}
