//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.Echo("statements before 'super' reference executed as expected");
super;
WScript.Echo("ERROR:statements after 'super' reference should not be executed");