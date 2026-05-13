// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Memory corruption mutator.
 */

'use strict';

const babelTypes = require('@babel/types');

const common = require('./common.js');
const random = require('../random.js');
const mutator = require('./mutator.js');

const MAX_CORRUPTIONS_PER_VAR = 3;
const MAX_UINT32 = 2 ** 32 - 1;

class MemoryCorruptionMutator extends mutator.Mutator {
  get visitor() {
    const thisMutator = this;

    return {
      ExpressionStatement(path) {
        if (!random.choose(thisMutator.settings.CORRUPT_MEMORY)) {
          return;
        }
        const variable = common.randomVariable(path);
        if (!variable) {
          return;
        }

        for (let i = 0; i < random.randInt(1, MAX_CORRUPTIONS_PER_VAR); i++) {
          thisMutator.insertAfterSkip(path, thisMutator.#generateCorruptCall(variable));
        }
      },
    };
  }

  #generateCorruptCall(variable) {
    const pathArray = this.#generateRandomPath();
    const offsetSeed = babelTypes.numericLiteral(random.randInt(0, MAX_UINT32));
    const size = babelTypes.numericLiteral(random.single([8, 16, 32]));

    const subFieldOffset = this.#calculateSubFieldOffset(size.value);

    const r = random.random();
    if (r < 0.1) {
      const builtinSeed = babelTypes.numericLiteral(random.randInt(0, MAX_UINT32));
      return this.#call('corruptFunction', [variable, pathArray, builtinSeed]);
    } else if (r < 0.2) {
      const bitPosition = babelTypes.numericLiteral(random.randInt(0, size.value - 1));
      return this.#call('corruptWithWorker', [
        variable, pathArray, offsetSeed, size, subFieldOffset, bitPosition
      ]);
    } else {
      const bitPosition = babelTypes.numericLiteral(random.randInt(0, size.value - 1));
      const magnitude = random.randInt(0, size.value);
      const upper = 1n << BigInt(magnitude);
      const lower = upper >> 1n;
      const randomMagnitudeValue = lower === upper ? lower : this.#randBigInt(lower, upper);

      const incrementValue = randomMagnitudeValue === 0n ? 1n : randomMagnitudeValue;

      const mutationType = random.single(['Bitflip', 'Increment', 'Replace']);
      const args = [variable, pathArray, offsetSeed, size, subFieldOffset];

      if (mutationType === 'Bitflip') {
        args.push(bitPosition);
      } else if (mutationType === 'Increment') {
        args.push(babelTypes.bigIntLiteral(incrementValue.toString()));
      } else {
        args.push(babelTypes.bigIntLiteral(randomMagnitudeValue.toString()));
      }

      return this.#call(`corruptDataWith${mutationType}`, args);
    }
  }

  #generateRandomPath() {
    const depth = random.randInt(0, 10);
    const elements = [];
    for (let i = 0; i < depth; i++) {
      const type = random.choose(0.25) ? 0 : 1; // 0: NEIGHBOR, 1: POINTER
      const seed = random.randInt(0, type === 0 ? 0xffff : MAX_UINT32);
      elements.push(babelTypes.arrayExpression([
        babelTypes.numericLiteral(type),
        babelTypes.numericLiteral(seed)
      ]));
    }
    return babelTypes.arrayExpression(elements);
  }

  #calculateSubFieldOffset(size) {
    if (size === 8) {
      return babelTypes.numericLiteral(random.randInt(0, 3));
    } else if (size === 16) {
      return babelTypes.numericLiteral(random.randInt(0, 1) * 2);
    }
    return babelTypes.numericLiteral(0);
  }

  #randBigInt(lower, upper) {
    const diff = upper - lower;
    const diffNum = Number(diff);
    if (diffNum < Number.MAX_SAFE_INTEGER) {
      return lower + BigInt(Math.floor(Math.random() * diffNum));
    }
    // Very simple fallback for larger ranges (rare in this context).
    return lower + (diff / 2n);
  }

  #call(name, args) {
    return babelTypes.expressionStatement(
      babelTypes.callExpression(babelTypes.identifier(name), args)
    );
  }
}

module.exports = {
  MemoryCorruptionMutator: MemoryCorruptionMutator,
};
