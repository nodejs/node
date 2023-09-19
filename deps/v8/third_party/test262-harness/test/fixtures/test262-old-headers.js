// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/**
 * The production Block { } in strict code can't contain function
 * declaration;
 *
 * @path bestPractice/Sbp_A1_T1.js
 * @description Trying to declare function at the Block statement
 * @onlyStrict
 * @negative SyntaxError
 * @bestPractice http://wiki.ecmascript.org/doku.php?id=conventions:no_non_standard_strict_decls
 */

"use strict";
{
    function __func(){}
}

