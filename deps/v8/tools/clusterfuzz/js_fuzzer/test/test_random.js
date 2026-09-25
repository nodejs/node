// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test random utilities.
 */

'use strict';

const assert = require('assert');
const sinon = require('sinon');

const helpers = require('./helpers.js');

const { sample, sampleOfTwo, twoBucketSample } = require('../random.js');

const sandbox = sinon.createSandbox();


describe('Random functions', () => {
  afterEach(() => {
    sandbox.restore();
  });

  it('twoBucketSample with one empty', () => {
    sandbox.stub(Math, 'random').callsFake(() => 0.5);
    assert.deepEqual([1, 2], twoBucketSample([0, 1, 2], [], 1, 2));
    assert.deepEqual([1, 2], twoBucketSample([], [0, 1, 2], 1, 2));
    assert.deepEqual([0], twoBucketSample([0], [], 1, 1));
    assert.deepEqual([0], twoBucketSample([], [0], 1, 1));
  });

  it('twoBucketSample chooses with 0.3', () => {
    sandbox.stub(Math, 'random').callsFake(() => 0.3);
    assert.deepEqual([1, 2], twoBucketSample([0, 1, 2], [3, 4, 5], 1, 2));
    // Higher factor.
    assert.deepEqual([3, 5], twoBucketSample([0, 1, 2], [3, 4, 5], 4, 2));
  });

  it('twoBucketSample chooses with 0.7', () => {
    sandbox.stub(Math, 'random').callsFake(() => 0.7);
    assert.deepEqual([4, 3], twoBucketSample([0, 1, 2], [3, 4, 5], 1, 2));
  });

  it('twoBucketSample chooses with 0.5', () => {
    sandbox.stub(Math, 'random').callsFake(() => 0.5);
    assert.deepEqual([3], twoBucketSample([0, 1], [2, 3, 4, 5], 1, 1));
    assert.deepEqual([3], twoBucketSample([0, 1, 2, 3], [4, 5], 1, 1));
    // Higher factor.
    assert.deepEqual([4], twoBucketSample([0, 1, 2, 3], [4, 5], 2, 1));
  });

  it('sampleOfTwo - low collision', () => {
    sandbox.stub(Math, 'random').callsFake(() => 0);
    assert.deepEqual([0, 1], sampleOfTwo([0, 1]));
    assert.deepEqual([0, 1], sampleOfTwo([0, 1, 2, 3, 4, 5]));
  });

  it('sampleOfTwo - high collision', () => {
    sandbox.stub(Math, 'random').callsFake(() => 0.99);
    assert.deepEqual([1, 0], sampleOfTwo([0, 1]));
    assert.deepEqual([5, 0], sampleOfTwo([0, 1, 2, 3, 4, 5]));
  });

  it('sampleOfTwo - no collision', () => {
    const stub = sandbox.stub(Math, 'random');
    stub.onCall(0).returns(0.7);
    stub.onCall(1).returns(0.2);
    stub.onCall(2).returns(0.7);
    stub.onCall(3).returns(0.2);
    assert.deepEqual([1, 0], sampleOfTwo([0, 1]));
    assert.deepEqual([4, 2], sampleOfTwo([0, 1, 2, 3, 4, 5]));
  });

  it('sample - pseudo random', () => {
    helpers.deterministicRandom(sandbox);
    assert.deepEqual([1], sample([0, 1], 1));
    assert.deepEqual([1, 0], sample([0, 1], 2));
    assert.deepEqual([5, 4, 0], sample([0, 1, 2, 3, 4, 5], 3));
    assert.deepEqual([1, 4, 2], sample([0, 1, 2, 3, 4, 5], 3));
  });
  sample
});
