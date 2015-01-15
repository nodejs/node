// Copyright 2010 The Closure Linter Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**
 * @fileoverview Test file for limited doc checks.
 */


/**
 * Don't require documentation of parameters.
 * @param {boolean}
 * @param {boolean} c
 * @param {boolean} d No check for punctuation
 * @bug 3259564
 */
x.y = function(a, b, c, d) {
  return a;
};
