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
 * @fileoverview Checks for extra goog.requires.
 *
 */

goog.require(''); // EXTRA_GOOG_REQUIRE
goog.require('dummy.Aa');
goog.require('dummy.Aa.CONSTANT'); // EXTRA_GOOG_REQUIRE
goog.require('dummy.Aa.Enum'); // EXTRA_GOOG_REQUIRE
goog.require('dummy.Bb');
goog.require('dummy.Ff'); // EXTRA_GOOG_REQUIRE
goog.require('dummy.Gg'); // EXTRA_GOOG_REQUIRE
goog.require('dummy.cc');
goog.require('dummy.cc'); // EXTRA_GOOG_REQUIRE
goog.require('dummy.hh'); // EXTRA_GOOG_REQUIRE

new dummy.Aa();
dummy.Bb.someMethod();
dummy.cc();
var x = dummy.Aa.Enum.VALUE;
var y = dummy.Aa.CONSTANT;
