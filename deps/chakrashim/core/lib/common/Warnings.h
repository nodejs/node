//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

//=============================
// Enabled Warnings
//=============================
#pragma warning(default:4242)   // conversion possible loss of data

//=============================
// Disabled Warnings
//=============================
// Warnings that we don't care about
#pragma warning(disable: 4100)  // unreferenced formal parameter
#pragma warning(disable: 4127)  // constant expression for Trace/Assert
#pragma warning(disable: 4200)  // nonstandard extension used: zero-sized array in struct/union
#pragma warning(disable: 4201)  // nameless unions are part of C++
#pragma warning(disable: 4512)  // private operator= are good to have
#pragma warning(disable: 4481)  // allow use of abstract and override keywords

#pragma warning(disable: 4324)  // structure was padded due to alignment specifier

// warnings caused by normal optimizations
#if DBG
#else // DBG
#pragma warning(disable: 4702)  // unreachable code caused by optimizations
#pragma warning(disable: 4189)  // initialized but unused variable
#pragma warning(disable: 4390)  // empty controlled statement
#endif // DBG

// PREFAST warnings
#ifdef _PREFAST_
// Warnings that we don't care about
#pragma warning(disable:6322)       // Empty _except block
#pragma warning(disable:6255)       // _alloca indicates failure by raising a stack overflow exception.  Consider using _malloca instead.
#pragma warning(disable:28112)      // A variable (processNativeCodeSize) which is accessed via an Interlocked function must always be accessed via an Interlocked function. See line 1024:  It is not always safe to access a variable which is accessed via the Interlocked* family of functions in any other way.
#pragma warning(disable:28159)      // Consider using 'GetTickCount64' instead of 'GetTickCount'. Reason: GetTickCount overflows roughly every 49 days.  Code that does not take that into account can loop indefinitely.  GetTickCount64 operates on 64 bit values and does not have that problem

#ifndef NTBUILD
// Would be nice to clean these up.
#pragma warning(disable:6054)       // String 'dumpName' might not be zero-terminated.
#pragma warning(disable:6244)       // Local declaration of 'Completed' hides previous declaration at line '76'
#pragma warning(disable:6246)       // Local declaration of 'i' hides declaration of the same name in outer scope.
#pragma warning(disable:6326)       // Potential comparison of a constant with another constant.
#endif
#endif // _PREFAST_
