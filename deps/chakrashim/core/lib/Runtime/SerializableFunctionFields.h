//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#ifdef DEFINE_ALL_FIELDS
#define DEFINE_FUNCTION_PROXY_FIELDS 1
#define DEFINE_PARSEABLE_FUNCTION_INFO_FIELDS 1
#define DEFINE_FUNCTION_BODY_FIELDS 1
#endif

// Default declaration for FunctionBody.h
#ifndef DECLARE_SERIALIZABLE_FIELD
#define DECLARE_SERIALIZABLE_FIELD(type, name, serializableType) type name
#endif

#ifndef DECLARE_MANUAL_SERIALIZABLE_FIELD
#define DECLARE_MANUAL_SERIALIZABLE_FIELD(type, name, serializableType, serializeHere) type name
#endif

#ifdef CURRENT_ACCESS_MODIFIER
#define PROTECTED_FIELDS protected:
#define PRIVATE_FIELDS   private:
#define PUBLIC_FIELDS    public:
#else
#define CURRENT_ACCESS_MODIFIER
#define PROTECTED_FIELDS
#define PRIVATE_FIELDS
#define PUBLIC_FIELDS
#endif

#if DEFINE_FUNCTION_PROXY_FIELDS
PROTECTED_FIELDS
    DECLARE_MANUAL_SERIALIZABLE_FIELD(uint, m_nestedCount, UInt32, false);

CURRENT_ACCESS_MODIFIER
#endif

#if DEFINE_PARSEABLE_FUNCTION_INFO_FIELDS
PROTECTED_FIELDS
    DECLARE_SERIALIZABLE_FIELD(ulong, m_grfscr, ULong);                 // For values, see fscr* values in scrutil.h.
    DECLARE_SERIALIZABLE_FIELD(ArgSlot, m_inParamCount, ArgSlot);         // Count of 'in' parameters to method
    DECLARE_SERIALIZABLE_FIELD(ArgSlot, m_reportedInParamCount, ArgSlot); // Count of 'in' parameters to method excluding default and rest
    DECLARE_SERIALIZABLE_FIELD(charcount_t, m_cchStartOffset, CharCount);   // offset in characters from the start of the document.
    DECLARE_SERIALIZABLE_FIELD(charcount_t, m_cchLength, CharCount);        // length of the function in code points (not bytes)
    DECLARE_SERIALIZABLE_FIELD(uint, m_cbLength, UInt32);              // length of the function in bytes

PUBLIC_FIELDS
    DECLARE_SERIALIZABLE_FIELD(UINT, scopeSlotArraySize, UInt32);

CURRENT_ACCESS_MODIFIER
#endif

#if DEFINE_FUNCTION_BODY_FIELDS
PUBLIC_FIELDS
    DECLARE_SERIALIZABLE_FIELD(RegSlot, m_varCount, RegSlot);           // Count of non-constant locals
    DECLARE_SERIALIZABLE_FIELD(RegSlot, m_constCount, RegSlot);         // Count of enregistered constants
    DECLARE_SERIALIZABLE_FIELD(RegSlot, m_firstTmpReg, RegSlot);
    DECLARE_SERIALIZABLE_FIELD(RegSlot, m_outParamMaxDepth, RegSlot);   // Count of call depth in a nested expression
    DECLARE_SERIALIZABLE_FIELD(uint, m_byteCodeCount, RegSlot);
    DECLARE_SERIALIZABLE_FIELD(uint, m_byteCodeWithoutLDACount, RegSlot);
    DECLARE_SERIALIZABLE_FIELD(uint, m_byteCodeInLoopCount, UInt32);
    DECLARE_SERIALIZABLE_FIELD(uint16, m_envDepth, UInt16);
    DECLARE_SERIALIZABLE_FIELD(uint16, m_argUsedForBranch, UInt16);

PRIVATE_FIELDS
    DECLARE_SERIALIZABLE_FIELD(ProfileId, profiledLdElemCount, UInt16);
    DECLARE_SERIALIZABLE_FIELD(ProfileId, profiledStElemCount, UInt16);
    DECLARE_SERIALIZABLE_FIELD(ProfileId, profiledCallSiteCount, UInt16);
    DECLARE_SERIALIZABLE_FIELD(ProfileId, profiledArrayCallSiteCount, UInt16);
    DECLARE_SERIALIZABLE_FIELD(ProfileId, profiledDivOrRemCount, UInt16);
    DECLARE_SERIALIZABLE_FIELD(ProfileId, profiledSwitchCount, UInt16);
    DECLARE_SERIALIZABLE_FIELD(ProfileId, profiledReturnTypeCount, UInt16);
    DECLARE_SERIALIZABLE_FIELD(ProfileId, profiledSlotCount, UInt16);
    DECLARE_SERIALIZABLE_FIELD(uint, loopCount, RegSlot);
    DECLARE_SERIALIZABLE_FIELD(FunctionBodyFlags, flags, FunctionBodyFlags);
    DECLARE_SERIALIZABLE_FIELD(bool, m_hasFinally, Bool);
    DECLARE_SERIALIZABLE_FIELD(bool, hasScopeObject, Bool);
    DECLARE_SERIALIZABLE_FIELD(bool, hasCachedScopePropIds, Bool);
    DECLARE_SERIALIZABLE_FIELD(uint, inlineCacheCount, UInt32);
    DECLARE_SERIALIZABLE_FIELD(uint, rootObjectLoadInlineCacheStart, UInt32);
    DECLARE_SERIALIZABLE_FIELD(uint, rootObjectLoadMethodInlineCacheStart, UInt32);
    DECLARE_SERIALIZABLE_FIELD(uint, rootObjectStoreInlineCacheStart, UInt32);
    DECLARE_SERIALIZABLE_FIELD(uint, isInstInlineCacheCount, UInt32);
    DECLARE_SERIALIZABLE_FIELD(uint, referencedPropertyIdCount, UInt32);
    DECLARE_SERIALIZABLE_FIELD(uint, objLiteralCount, UInt32);
    DECLARE_SERIALIZABLE_FIELD(uint, literalRegexCount, UInt32);
    DECLARE_SERIALIZABLE_FIELD(uint, innerScopeCount, UInt32);
    DECLARE_SERIALIZABLE_FIELD(RegSlot, localClosureRegister, RegSlot);
    DECLARE_SERIALIZABLE_FIELD(RegSlot, localFrameDisplayRegister, RegSlot);
    DECLARE_SERIALIZABLE_FIELD(RegSlot, envRegister, RegSlot);
    DECLARE_SERIALIZABLE_FIELD(RegSlot, thisRegisterForEventHandler, RegSlot);
    DECLARE_SERIALIZABLE_FIELD(RegSlot, firstInnerScopeRegister, RegSlot);
    DECLARE_SERIALIZABLE_FIELD(RegSlot, funcExprScopeRegister, RegSlot);

CURRENT_ACCESS_MODIFIER
#endif

#undef DEFINE_ALL_FIELDS
#undef DEFINE_FUNCTION_PROXY_FIELDS
#undef DEFINE_PARSEABLE_FUNCTION_INFO_FIELDS
#undef DEFINE_FUNCTION_BODY_FIELDS
#undef CURRENT_ACCESS_MODIFIER
#undef DECLARE_MANUAL_SERIALIZABLE_FIELD
#undef DECLARE_SERIALIZABLE_FIELD
#undef PROTECTED_FIELDS
#undef PRIVATE_FIELDS
#undef PUBLIC_FIELDS
