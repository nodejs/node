//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var a=true;
// arguments used as a function name in strict mode
(function(){if(a) { function bar3() {"use strict"; function arguments(a) {} }; bar3();}})();