//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
// This file defines String.prototype tag functions and their parameters.

#ifndef TAGENTRY
#error "TAGENTRY macro isn't defined!"
#define TAGENTRY(...) // Stop editors from seeing this as one big syntax error
#endif

//       Name       Tag         Properties
TAGENTRY(Anchor,    L"a",       L"name" )
TAGENTRY(Big,       L"big"              )
TAGENTRY(Blink,     L"blink"            )
TAGENTRY(Bold,      L"b"                )
TAGENTRY(Fixed,     L"tt"               )
TAGENTRY(FontColor, L"font",    L"color")
TAGENTRY(FontSize,  L"font",    L"size" )
TAGENTRY(Italics,   L"i"                )
TAGENTRY(Link,      L"a",       L"href" )
TAGENTRY(Small,     L"small"            )
TAGENTRY(Strike,    L"strike"           )
TAGENTRY(Sub,       L"sub"              )
TAGENTRY(Sup,       L"sup"              )
