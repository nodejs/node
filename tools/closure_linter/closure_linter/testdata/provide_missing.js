// Copyright 2011 The Closure Linter Authors. All Rights Reserved.
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

// We are missing a provide of goog.something.Else.
// -15: MISSING_GOOG_PROVIDE

/**
 * @fileoverview Tests missing provides and the usage of the missing provide
 * suppression annotation.
 *
 */



/**
 * Constructor for Something.
 * @constructor
 * @suppress {missingProvide}
 */
goog.something.Something = function() {};



/**
 * Constructor for Else. We should get an error about providing this, but not
 * about the constructor for Something.
 * @constructor
 */
goog.something.Else = function() {};
