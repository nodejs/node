// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

load('test/mjsunit/test-async.js');

// .return() from state suspendedStart with undefined
testAsync(test => {
  test.plan(3);

  async function* gen() {
    test.unreachable();
  }

  let didResolvePromise = false;
  let g = gen();
  g.return(undefined).then(
      (iterResult) => {
        test.equals(true, didResolvePromise);
        test.equals({ value: undefined, done: true }, iterResult);
      },
      test.unexpectedRejection());

  g.next().then(
      (iterResult) => {
        test.equals({ value: undefined, done: true }, iterResult);
      },
      test.unexpectedRejection());

  // Race: generator request's promise should resolve after this Promise.
  Promise.resolve("already-resolved").then(
      _ => { didResolvePromise = true },
      test.unexpectedRejection());

}, "AsyncGenerator.return(undefined) / suspendStart");

// .return() from state suspendedStart with thenable
testAsync(test => {
  test.plan(3);

  async function* gen() {
    test.unreachable();
  }

  let didResolvePromise = false;
  let g = gen();

  let resolve;
  let awaitedThenable = { then(resolveFn) { resolve = resolveFn; } };

  g.return(awaitedThenable).then(
      (iterResult) => {
        test.equals(true, didResolvePromise);
        test.equals({ value: "resolvedPromise", done: true }, iterResult);
      },
      test.unexpectedRejection());

  g.next().then(
      (iterResult) => {
        test.equals({ value: undefined, done: true }, iterResult);
      },
      test.unexpectedRejection());

  test.drainMicrotasks();
  resolve("resolvedPromise");

  // Race: generator request's promise should resolve after this Promise.
  Promise.resolve("already-resolved").then(
      _ => { didResolvePromise = true },
      test.unexpectedRejection());

}, "AsyncGenerator.return(thenable) / suspendStart");

// .return() from state suspendedYield with undefined
testAsync(test => {
  test.plan(4);

  async function* gen() {
    yield;
    test.unreachable();
  }

  let didResolvePromise = false;
  let g = gen();
  g.next().then(
      (iterResult) => {
        test.equals({ value: undefined, done: false }, iterResult);
      },
      test.unexpectedRejection());
  g.return(undefined).then(
      (iterResult) => {
        test.equals(true, didResolvePromise);
        test.equals({ value: undefined, done: true }, iterResult);
      },
      test.unexpectedRejection());
  g.next().then(
      (iterResult) => {
        test.equals({ value: undefined, done: true }, iterResult);
      },
      test.unexpectedRejection());

  // Race: generator request's promise should resolve after this Promise.
  Promise.resolve("already-resolved").then(
      _ => { didResolvePromise = true },
      test.unexpectedRejection());

}, "AsyncGenerator.return(undefined) / suspendedYield");

// .return() from state suspendedYield with thenable
testAsync(test => {
  test.plan(4);

  async function* gen() {
    yield;
    test.unreachable();
  }

  let didResolvePromise = false;
  let g = gen();

  let resolve;
  let awaitedThenable = { then(resolveFn) { resolve = resolveFn; } };

  g.next().then(
      (iterResult) => {
        test.equals({ value: undefined, done: false }, iterResult);
      },
      test.unexpectedRejection());
  g.return(awaitedThenable).then(
      (iterResult) => {
        test.equals(true, didResolvePromise);
        test.equals({ value: "resolvedPromise", done: true }, iterResult);
      },
      test.unexpectedRejection());

  g.next().then(
      (iterResult) => {
        test.equals({ value: undefined, done: true }, iterResult);
      },
      test.unexpectedRejection());

  test.drainMicrotasks();
  resolve("resolvedPromise");

  // Race: generator request's promise should resolve after this Promise.
  Promise.resolve("already-resolved").then(
      _ => { didResolvePromise = true },
      test.unexpectedRejection());

}, "AsyncGenerator.return(thenable) / suspendYield");
