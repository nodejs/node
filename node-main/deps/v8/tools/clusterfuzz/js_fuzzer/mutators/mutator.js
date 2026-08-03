// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Mutator
 */
'use strict';

const babelTraverse = require('@babel/traverse').default;
const babelTypes = require('@babel/types');

const SKIP_LARGE_LOOP_MUTATION_PROB = 0.75;

/**
 * Container for any state that lives throughout one fuzz-test output
 * generation. Can be used to collect information during parsing and
 * use it later in mutators.
 */
class MutationContext {
  constructor () {
    this.asyncFunctions = new Set();
    this.infiniteFunctions = new Set();
    this.extraResources = new Set();
    this.loopVariables = new Set();
  }
}

const EMPTY_DEFAULT_CONTEXT = new MutationContext();

class Mutator {
  constructor(settings) {
    this.settings = settings;
    this.context = EMPTY_DEFAULT_CONTEXT;
  }

  get visitor() {
    return null;
  }

  _traverse(ast, visitor) {
    let oldEnter = null;
    if (Object.prototype.hasOwnProperty.call(visitor, 'enter')) {
      oldEnter = visitor['enter'];
    }

    // Transparently skip nodes that are marked.
    visitor['enter'] = (path) => {
      if (this.shouldSkip(path.node)) {
        path.skip();
        return;
      }

      if (oldEnter) {
        oldEnter(path);
      }
    }

    babelTraverse(ast, visitor);
  }

  mutate(source, context=EMPTY_DEFAULT_CONTEXT) {
    this.context = context;
    const result = this.visitor;
    if (Array.isArray(result)) {
      for (const visitor of result) {
        this._traverse(source.ast, visitor);
      }
    } else {
      this._traverse(source.ast, result);
    }
  }

  get _skipPropertyName() {
    return '__skip' + this.constructor.name;
  }

  shouldSkip(node) {
    return Boolean(node[this._skipPropertyName]);
  }

  skipMutations(node) {
    // Mark a node to skip further mutations of the same kind.
    if (Array.isArray(node)) {
      for (const item of node) {
        item[this._skipPropertyName] = true;
      }
    } else {
      node[this._skipPropertyName] = true;
    }

    return node;
  }

  insertBeforeSkip(path, node) {
    this.skipMutations(node);
    path.insertBefore(node);
  }

  insertAfterSkip(path, node) {
    this.skipMutations(node);
    path.insertAfter(node);
  }

  replaceWithSkip(path, node) {
    this.skipMutations(node);
    path.replaceWith(node);
  }

  replaceWithMultipleSkip(path, node) {
    this.skipMutations(node);
    path.replaceWithMultiple(node);
  }

  annotate(node, message) {
    babelTypes.addComment(
        node, 'leading', ` ${this.constructor.name}: ${message} `);
  }
}

module.exports = {
  SKIP_LARGE_LOOP_MUTATION_PROB: SKIP_LARGE_LOOP_MUTATION_PROB,
  Mutator: Mutator,
  MutationContext: MutationContext,
}
