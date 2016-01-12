//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var a=true;
// eval declared as a var in strict mode
(function(){if(a){function bar() {"use strict"; var x = function() { var eval=print(); };};bar();}})();