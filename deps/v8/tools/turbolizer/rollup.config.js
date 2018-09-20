// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import typescript from 'rollup-plugin-typescript2';
import node from 'rollup-plugin-node-resolve';

export default {
  entry: "src/turbo-visualizer.ts",
  format: "iife",
  plugins: [node(), typescript({abortOnError:false})],
  dest: "build/turbolizer.js"
};
