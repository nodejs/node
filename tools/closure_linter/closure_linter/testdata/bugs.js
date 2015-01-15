// Copyright 2007 The Closure Linter Authors. All Rights Reserved.
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

// A file full of known bugs - this file serves only as a reference and is not
// tested in any way.

/**
 * @param {{foo} x This is a bad record type.
 * @param {{foo}} y This is a good record type with bad spacing.
 * @param {{foo}} This is a good record type with no parameter name.
 */
function f(x, y, z) {
}


// Should report extra space errors.
var magicProps = { renderRow: 0 };

// No error reported here for missing space before {.
if (x){
}

// Should have a "brace on wrong line" error.
if (x)
{
}

// We could consider not reporting it when wrapping makes it necessary, as in:
if (aLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongCondition)
    {
  // Code here.
}
