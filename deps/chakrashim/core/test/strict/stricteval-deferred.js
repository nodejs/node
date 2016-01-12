//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var a=true;
// eval declared as a formal in strict mode
(function(){if(a){function foo() {"use strict"; var x = function(eval) { print(eval); };};foo();}})();