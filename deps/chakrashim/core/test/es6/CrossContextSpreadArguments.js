//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var simpleIterator = function(args) {
     var iter = (function(args) {
     var j = 0;
     var length = args.length;
     return function Iterator() {
     return {next: function() {
     
        if (j < length)
        {
            return { value: args[j++], done: false };
        }
        else
        {
            j = 0;
            return { done: true };
        }
     }}}})(args)
     return iter;};

var a = [1,2,3];
a[Symbol.iterator] = simpleIterator(a);