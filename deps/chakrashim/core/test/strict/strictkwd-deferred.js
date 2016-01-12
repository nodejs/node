//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Bug 53107 - Syntax error on deferred re-parse
yoyo=[];
yoyo[(function  yield (x) { "use strict"; } )()];