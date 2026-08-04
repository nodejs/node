// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Memory corruption mutator.
 */

'use strict';

const babelTemplate = require('@babel/template').default;
const babelTypes = require('@babel/types');

const common = require('./common.js');
const random = require('../random.js');
const mutator = require('./mutator.js');

const CORRUPT = babelTemplate('corrupt(VAR, SEED);');

const MAX_CORRUPTIONS_PER_VAR = 3;
const MAX_SEED = 2 ** 32 -1;

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
        for(let i = 0; i < random.randInt(1, MAX_CORRUPTIONS_PER_VAR); i++) {
          const seed = babelTypes.numericLiteral(random.randInt(0, MAX_SEED));
          const insert = CORRUPT({VAR: variable, SEED: seed});
          thisMutator.insertAfterSkip(path, insert);
        }
      },
    };
  }
}

module.exports = {
  MemoryCorruptionMutator: MemoryCorruptionMutator,
};
