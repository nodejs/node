// Copyright (C) 2018 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import commonjs from 'rollup-plugin-commonjs';
import nodeResolve from 'rollup-plugin-node-resolve';

export default {
  output: {name: 'perfetto'},
  plugins:
      [
        nodeResolve({
          mainFields: ['browser'],
          browser: true,
          preferBuiltins: false,
        }),

        // emscripten conditionally executes require('fs') (likewise for
        // others), when running under node. Rollup can't find those libraries
        // so expects these to be present in the global scope, which then fails
        // at runtime. To avoid this we ignore require('fs') and the like.
        commonjs({
          ignore: [
            'fs',
            'path',
            'crypto',
          ]
        }),
      ],
}
