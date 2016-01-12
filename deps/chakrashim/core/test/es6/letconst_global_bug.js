//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Bug OS 101821 (BLUE 588397)

let x = 5;
// If the inline slot capacity of the GlobalObject was increased (to at least 116?)
// then this assignment, this.x = 10, ended up throwing a Use Before Declaration error
// which, of course, is incorrect.  It should allow it and create a property on the
// global object named x which is then shadowed by the let variable for naked accesses
// to x (root accesses).
this.x = 10;

WScript.Echo("pass");
