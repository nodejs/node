//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// This is a sketchy test because it relies on knowing that the following pattern
// hits the heuristics that decide to cache the scopes within the generator function.
// If those heuristics were to change this test could no longer be hitting the
// desired code paths.  If you suspect it is not hitting the cached scope code paths
// dump the bytecode and check for InitCachedScope and LdHeapArgsCached opcodes.
var o = {
    gf: function* () {
        var _a = 'pas';
        function a() { return _a; }
        return eval('a()') + arguments[0];
    }
};

function test() {
    for (var i = 0; i < 3; i += 1) {
        var g = o.gf('sed');
        WScript.Echo(g.next().value);
    }
}

test();
