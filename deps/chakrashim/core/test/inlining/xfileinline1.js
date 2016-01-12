//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function foo()
{
  this.a = 5;
  this.b = 2;
}
foo.prototype.add = function(x,y) 
{
  return x+y;
}
