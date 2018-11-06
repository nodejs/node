// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import typescript from 'rollup-plugin-typescript2';
import node from 'rollup-plugin-node-resolve';

export default {
  input: "src/turbo-visualizer.ts",
  plugins: [node(), typescript({abortOnError:false})],
  output: {file: "build/turbolizer.js", format: "iife", sourcemap: true}
};
