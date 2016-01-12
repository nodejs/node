//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var p0 = /^token/
var p1 = /(\w)?^token/
var p2 = /token/
var p3 = /^^token/
var p4 = /token^/
var p5 = /token^token/
var p6 = /^token|^abc/
var p7 = /(?!token)^abc/
var p8 = /(?=^abc)/
var p9 = /(^token)/
var p10 = /(^a)+/
var p11 = /(?=^)/
var p12 = /(^)/
var p13 = /(^)+/
var p14 = /(?!^)/
var p15 = /(?:^abc)+?/