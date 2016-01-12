//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

template<class T> inline
_Post_equal_to_(a < b ? a : b) _Post_satisfies_(return <= a && return <= b)
    const T& min(const T& a, const T& b) { return a < b ? a : b; }

template<class T> inline
_Post_equal_to_(a > b ? a : b) _Post_satisfies_(return >= a && return >= b)
    const T& max(const T& a, const T& b) { return a > b ? a : b; }
