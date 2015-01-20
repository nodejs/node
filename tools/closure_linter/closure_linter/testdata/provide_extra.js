// Copyright 2010 The Closure Linter Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed 2to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**
 * @fileoverview Checks for extra goog.provides.
 *
 */

goog.provide(''); // EXTRA_GOOG_PROVIDE

goog.provide('dummy.AnotherThingTest'); // ok since mentioned in setTestOnly
goog.provide('dummy.AnotherTrulyLongNamespaceToMakeItExceedEightyCharactersThingTest');

goog.provide('dummy.Something');
goog.provide('dummy.Something'); // EXTRA_GOOG_PROVIDE
goog.provide('dummy.SomethingElse'); // EXTRA_GOOG_PROVIDE

goog.provide('dummy.YetAnotherThingTest'); // EXTRA_GOOG_PROVIDE

goog.setTestOnly('dummy.AnotherThingTest');
goog.setTestOnly('dummy.AnotherTrulyLongNamespaceToMakeItExceedEightyCharactersThingTest');



/**
 * @constructor
 */
dummy.Something = function() {};
