//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
// This is the list of internal properties used in the Chakra engine.
// They become nameless compile time known PropertyRecords, stored as static
// fields on the InternalPropertyRecords class.

INTERNALPROPERTY(TypeOfPrototypObject)  // Used to store the type of the prototype object in the prototype objects slots
INTERNALPROPERTY(NonExtensibleType)     // Used to store shared non-extensible type in PathTypeHandler::propertySuccessors map.
INTERNALPROPERTY(SealedType)            // Used to store shared sealed type in PathTypeHandler::propertySuccessors map.
INTERNALPROPERTY(FrozenType)            // Used to store shared frozen type in PathTypeHandler::propertySuccessors map.
INTERNALPROPERTY(StackTrace)            // Stack trace object for Error.stack generation
INTERNALPROPERTY(StackTraceCache)       // Cache of Error.stack string
INTERNALPROPERTY(WeakMapKeyMap)         // WeakMap data stored on WeakMap key objects
INTERNALPROPERTY(HiddenObject)          // Used to store hidden data for JS library code (Intl as an example will use this)
INTERNALPROPERTY(RevocableProxy)        // Internal slot for [[RevokableProxy]] for revocable proxy in ES6
INTERNALPROPERTY(MutationBp)            // Used to store strong reference to the mutation breakpoint object
#undef INTERNALPROPERTY
