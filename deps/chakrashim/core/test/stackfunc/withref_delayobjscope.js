//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var node = {};
function _dmcn(type) {
    if (type) {
        function findUL(recurse) {
            with(node) {
                if (recurse)
                {
                    findUL(false);
                }
            }
        }
        findUL(false)
    }
}
 
_dmcn(true);
