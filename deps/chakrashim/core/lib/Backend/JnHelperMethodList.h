//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#ifndef HELPERCALL
#error HELPERCALL must be defined before including this file
#else

#define HELPERCALL_FULL_OR_INPLACE(Name, Address, Attributes) \
    HELPERCALL(Name##, Address##, Attributes) \
    HELPERCALL(Name##_Full, Address##_Full, Attributes) \
    HELPERCALL(Name##InPlace, Address##_InPlace, Attributes)

#if defined(SSE2MATH)
#define HELPERCALL_MATH(Name, Address, SSE2Address, Attributes) \
    HELPERCALL(Name##, SSE2Address##, Attributes)
#define HELPERCALL_FULL_OR_INPLACE_MATH(Name, Address, SSE2Address, Attributes) \
    HELPERCALL_FULL_OR_INPLACE(Name##, SSE2Address##, Attributes)
#else
#define HELPERCALL_MATH(Name, Address, SSE2Address, Attributes) \
    HELPERCALL(Name##, Address##, Attributes)
#define HELPERCALL_FULL_OR_INPLACE_MATH(Name, Address, SSE2Address, Attributes) \
    HELPERCALL_FULL_OR_INPLACE(Name##, Address##, Attributes)
#endif

//HELPERCALL(Name, Address, Attributes)

HELPERCALL(Invalid, nullptr, 0)

HELPERCALL(ScrFunc_OP_NewScFunc, Js::ScriptFunction::OP_NewScFunc, 0)
HELPERCALL(ScrFunc_OP_NewScGenFunc, Js::JavascriptGeneratorFunction::OP_NewScGenFunc, 0)
HELPERCALL(ScrFunc_CheckAlignment, Js::JavascriptFunction::CheckAlignment, 0)
HELPERCALL(ScrObj_LdHandlerScope, Js::JavascriptOperators::OP_LdHandlerScope, 0)
HELPERCALL(ScrObj_LdFrameDisplay, Js::JavascriptOperators::OP_LdFrameDisplay, 0)
HELPERCALL(ScrObj_LdFrameDisplayNoParent, Js::JavascriptOperators::OP_LdFrameDisplayNoParent, 0)
HELPERCALL(ScrObj_LdStrictFrameDisplay, Js::JavascriptOperators::OP_LdStrictFrameDisplay, 0)
HELPERCALL(ScrObj_LdStrictFrameDisplayNoParent, Js::JavascriptOperators::OP_LdStrictFrameDisplayNoParent, 0)
HELPERCALL(ScrObj_LdInnerFrameDisplay, Js::JavascriptOperators::OP_LdInnerFrameDisplay, 0)
HELPERCALL(ScrObj_LdInnerFrameDisplayNoParent, Js::JavascriptOperators::OP_LdInnerFrameDisplayNoParent, 0)
HELPERCALL(ScrObj_LdStrictInnerFrameDisplay, Js::JavascriptOperators::OP_LdStrictInnerFrameDisplay, 0)
HELPERCALL(ScrObj_LdStrictInnerFrameDisplayNoParent, Js::JavascriptOperators::OP_LdStrictInnerFrameDisplayNoParent, 0)
HELPERCALL(ScrObj_OP_IsInst, Js::JavascriptOperators::OP_IsInst, AttrCanThrow)

HELPERCALL(Op_IsIn, Js::JavascriptOperators::IsIn, AttrCanThrow)
HELPERCALL(Op_IsObject, Js::JavascriptOperators::IsObject, AttrCanThrow)
HELPERCALL(Op_IsClassConstructor, Js::JavascriptOperators::IsClassConstructor, AttrCanThrow)
HELPERCALL(Op_LoadHeapArguments, Js::JavascriptOperators::LoadHeapArguments, 0)
HELPERCALL(Op_LoadHeapArgsCached, Js::JavascriptOperators::LoadHeapArgsCached, 0)
HELPERCALL(OP_InitCachedScope, Js::JavascriptOperators::OP_InitCachedScope, 0)
HELPERCALL(OP_InitCachedFuncs, Js::JavascriptOperators::OP_InitCachedFuncs, 0)
HELPERCALL(OP_InvalidateCachedScope, Js::JavascriptOperators::OP_InvalidateCachedScope, 0)
HELPERCALL(OP_NewScopeObject, Js::JavascriptOperators::OP_NewScopeObject, 0)
HELPERCALL(OP_NewScopeSlots, Js::JavascriptOperators::OP_NewScopeSlots, 0)
HELPERCALL(OP_NewScopeSlotsWithoutPropIds, Js::JavascriptOperators::OP_NewScopeSlotsWithoutPropIds, 0)
HELPERCALL(OP_NewBlockScope, Js::JavascriptOperators::OP_NewBlockScope, 0)
HELPERCALL(OP_NewPseudoScope, Js::JavascriptOperators::OP_NewPseudoScope, 0)
HELPERCALL(OP_CloneInnerScopeSlots, Js::JavascriptOperators::OP_CloneScopeSlots, 0)
HELPERCALL(OP_CloneBlockScope, Js::JavascriptOperators::OP_CloneBlockScope, 0)
HELPERCALL(LdThis, Js::JavascriptOperators::OP_GetThis, 0)
HELPERCALL(LdThisNoFastPath, Js::JavascriptOperators::OP_GetThisNoFastPath, 0)
HELPERCALL(StrictLdThis, Js::JavascriptOperators::OP_StrictGetThis, 0)
HELPERCALL(Op_LdElemUndef, Js::JavascriptOperators::OP_LoadUndefinedToElement, 0)
HELPERCALL(Op_LdElemUndefDynamic, Js::JavascriptOperators::OP_LoadUndefinedToElementDynamic, 0)
HELPERCALL(Op_LdElemUndefScoped, Js::JavascriptOperators::OP_LoadUndefinedToElementScoped, 0)
HELPERCALL(Op_EnsureNoRootProperty, Js::JavascriptOperators::OP_EnsureNoRootProperty, AttrCanThrow)
HELPERCALL(Op_EnsureNoRootRedeclProperty, Js::JavascriptOperators::OP_EnsureNoRootRedeclProperty, AttrCanThrow)
HELPERCALL(Op_EnsureNoRedeclPropertyScoped, Js::JavascriptOperators::OP_ScopedEnsureNoRedeclProperty, AttrCanThrow)

HELPERCALL(Op_ToSpreadedFunctionArgument, Js::JavascriptOperators::OP_LdCustomSpreadIteratorList, AttrCanThrow)
HELPERCALL(Op_ConvObject, Js::JavascriptOperators::ToObject, AttrCanThrow)
HELPERCALL(Op_NewWithObject, Js::JavascriptOperators::ToWithObject, AttrCanThrow)
HELPERCALL(SetComputedNameVar, Js::JavascriptOperators::OP_SetComputedNameVar, 0)
HELPERCALL(Op_UnwrapWithObj, Js::JavascriptOperators::OP_UnwrapWithObj, 0)
HELPERCALL(Op_ConvNumber_Full, Js::JavascriptOperators::ToNumber, AttrCanThrow)
HELPERCALL(Op_ConvNumberInPlace, Js::JavascriptOperators::ToNumberInPlace, AttrCanThrow)
HELPERCALL(Op_ConvNumber_Helper, Js::JavascriptConversion::ToNumber_Helper, 0)
HELPERCALL(Op_ConvFloat_Helper, Js::JavascriptConversion::ToFloat_Helper, 0)
HELPERCALL(Op_ConvNumber_FromPrimitive, Js::JavascriptConversion::ToNumber_FromPrimitive, 0)
HELPERCALL(Op_Typeof, Js::JavascriptOperators::Typeof, 0)
HELPERCALL(Op_TypeofElem, Js::JavascriptOperators::TypeofElem, AttrCanThrow)
HELPERCALL(Op_TypeofElem_UInt32, Js::JavascriptOperators::TypeofElem_UInt32, 0)
HELPERCALL(Op_TypeofElem_Int32, Js::JavascriptOperators::TypeofElem_Int32, 0)
HELPERCALL(Op_TypeofPropertyScoped, Js::JavascriptOperators::OP_TypeofPropertyScoped, 0)
HELPERCALL(Op_Rem_Double, Js::NumberUtilities::Modulus, 0)

HELPERCALL_FULL_OR_INPLACE_MATH(Op_Increment, Js::JavascriptMath::Increment, Js::SSE2::JavascriptMath::Increment, AttrCanThrow)
HELPERCALL_FULL_OR_INPLACE_MATH(Op_Decrement, Js::JavascriptMath::Decrement, Js::SSE2::JavascriptMath::Decrement, AttrCanThrow)
HELPERCALL_FULL_OR_INPLACE_MATH(Op_Negate, Js::JavascriptMath::Negate, Js::SSE2::JavascriptMath::Negate, AttrCanThrow)
HELPERCALL_FULL_OR_INPLACE_MATH(Op_Not, Js::JavascriptMath::Not, Js::SSE2::JavascriptMath::Not, AttrCanThrow)

HELPERCALL_MATH(Op_AddLeftDead, Js::JavascriptMath::AddLeftDead, Js::SSE2::JavascriptMath::AddLeftDead, AttrCanThrow)
HELPERCALL_FULL_OR_INPLACE_MATH(Op_Add, Js::JavascriptMath::Add, Js::SSE2::JavascriptMath::Add, AttrCanThrow)
HELPERCALL_FULL_OR_INPLACE_MATH(Op_Divide, Js::JavascriptMath::Divide, Js::SSE2::JavascriptMath::Divide, AttrCanThrow)
HELPERCALL_FULL_OR_INPLACE_MATH(Op_Modulus, Js::JavascriptMath::Modulus, Js::SSE2::JavascriptMath::Modulus, AttrCanThrow)
HELPERCALL_FULL_OR_INPLACE_MATH(Op_Multiply, Js::JavascriptMath::Multiply, Js::SSE2::JavascriptMath::Multiply, AttrCanThrow)
HELPERCALL_FULL_OR_INPLACE_MATH(Op_Subtract, Js::JavascriptMath::Subtract, Js::SSE2::JavascriptMath::Subtract, AttrCanThrow)
HELPERCALL_FULL_OR_INPLACE_MATH(Op_Exponentiation, Js::JavascriptMath::Exponentiation, Js::SSE2::JavascriptMath::Exponentiation, AttrCanThrow)


HELPERCALL_FULL_OR_INPLACE_MATH(Op_And, Js::JavascriptMath::And, Js::SSE2::JavascriptMath::And, AttrCanThrow)
HELPERCALL_FULL_OR_INPLACE_MATH(Op_Or, Js::JavascriptMath::Or, Js::SSE2::JavascriptMath::Or, AttrCanThrow)
HELPERCALL_FULL_OR_INPLACE_MATH(Op_Xor, Js::JavascriptMath::Xor, Js::SSE2::JavascriptMath::Xor, AttrCanThrow)

HELPERCALL_MATH(Op_MulAddLeft, Js::JavascriptMath::MulAddLeft, Js::SSE2::JavascriptMath::MulAddLeft, AttrCanThrow)
HELPERCALL_MATH(Op_MulAddRight, Js::JavascriptMath::MulAddRight, Js::SSE2::JavascriptMath::MulAddRight, AttrCanThrow)
HELPERCALL_MATH(Op_MulSubLeft, Js::JavascriptMath::MulSubLeft, Js::SSE2::JavascriptMath::MulSubLeft, AttrCanThrow)
HELPERCALL_MATH(Op_MulSubRight, Js::JavascriptMath::MulSubRight, Js::SSE2::JavascriptMath::MulSubRight, AttrCanThrow)

HELPERCALL_MATH(Op_ShiftLeft, Js::JavascriptMath::ShiftLeft, Js::SSE2::JavascriptMath::ShiftLeft, AttrCanThrow)
HELPERCALL_MATH(Op_ShiftLeft_Full, Js::JavascriptMath::ShiftLeft_Full, Js::SSE2::JavascriptMath::ShiftLeft_Full, AttrCanThrow)
HELPERCALL_MATH(Op_ShiftRight, Js::JavascriptMath::ShiftRight, Js::SSE2::JavascriptMath::ShiftRight, AttrCanThrow)
HELPERCALL_MATH(Op_ShiftRight_Full, Js::JavascriptMath::ShiftRight_Full, Js::SSE2::JavascriptMath::ShiftRight_Full, AttrCanThrow)
HELPERCALL_MATH(Op_ShiftRightU, Js::JavascriptMath::ShiftRightU, Js::SSE2::JavascriptMath::ShiftRightU, AttrCanThrow)
HELPERCALL_MATH(Op_ShiftRightU_Full, Js::JavascriptMath::ShiftRightU_Full, Js::SSE2::JavascriptMath::ShiftRightU_Full, AttrCanThrow)

HELPERCALL_MATH(Conv_ToInt32_Full, Js::JavascriptMath::ToInt32_Full, Js::SSE2::JavascriptMath::ToInt32_Full, AttrCanThrow)
HELPERCALL_MATH(Conv_ToInt32, (int32 (*)(Js::Var, Js::ScriptContext *))Js::JavascriptMath::ToInt32, (int32 (*)(Js::Var, Js::ScriptContext *))Js::SSE2::JavascriptMath::ToInt32, AttrCanThrow)

HELPERCALL_MATH(Op_FinishOddDivByPow2, Js::JavascriptMath::FinishOddDivByPow2, Js::SSE2::JavascriptMath::FinishOddDivByPow2, 0)
HELPERCALL_MATH(Op_FinishOddDivByPow2InPlace, Js::JavascriptMath::FinishOddDivByPow2_InPlace, Js::SSE2::JavascriptMath::FinishOddDivByPow2_InPlace, 0)
HELPERCALL_MATH(Conv_ToInt32Core, (int32 (*)(double))Js::JavascriptMath::ToInt32Core, (int32 (*)(double))Js::SSE2::JavascriptMath::ToInt32Core, 0)
HELPERCALL_MATH(Conv_ToUInt32Core, (uint32(*)(double))Js::JavascriptMath::ToUInt32, (uint32(*)(double))Js::SSE2::JavascriptMath::ToUInt32, 0)
HELPERCALL_MATH(Op_MaxInAnArray, Js::JavascriptMath::MaxInAnArray, Js::SSE2::JavascriptMath::MaxInAnArray, AttrCanThrow)
HELPERCALL_MATH(Op_MinInAnArray, Js::JavascriptMath::MinInAnArray, Js::SSE2::JavascriptMath::MinInAnArray, AttrCanThrow)

HELPERCALL(Op_ConvString, Js::JavascriptConversion::ToString, AttrCanThrow)
HELPERCALL(Op_CoerseString, Js::JavascriptConversion::CoerseString, AttrCanThrow)
HELPERCALL(Op_CoerseRegex, (Js::JavascriptRegExp* (*) (Js::Var aValue, Js::Var options, Js::ScriptContext *scriptContext))Js::JavascriptRegExp::CreateRegEx, AttrCanThrow)

HELPERCALL(Op_ConvPrimitiveString, Js::JavascriptConversion::ToPrimitiveString, AttrCanThrow)
HELPERCALL(Op_CompoundStringCloneForConcat, Js::CompoundString::JitClone, AttrCanThrow)
HELPERCALL(Op_CompoundStringCloneForAppending, Js::CompoundString::JitCloneForAppending, AttrCanThrow)

HELPERCALL(Op_Equal, Js::JavascriptOperators::Equal, 0)
HELPERCALL(Op_Equal_Full, Js::JavascriptOperators::Equal_Full, 0)
HELPERCALL(Op_Greater, Js::JavascriptOperators::Greater, AttrCanThrow)
HELPERCALL(Op_Greater_Full, Js::JavascriptOperators::Greater_Full, AttrCanThrow)
HELPERCALL(Op_GreaterEqual, Js::JavascriptOperators::GreaterEqual, AttrCanThrow)
HELPERCALL(Op_Less, Js::JavascriptOperators::Less, AttrCanThrow)
HELPERCALL(Op_LessEqual, Js::JavascriptOperators::LessEqual, AttrCanThrow)
HELPERCALL(Op_NotEqual, Js::JavascriptOperators::NotEqual, 0)
HELPERCALL(Op_StrictEqual, Js::JavascriptOperators::StrictEqual, 0)
HELPERCALL(Op_StrictEqualString, Js::JavascriptOperators::StrictEqualString, 0)
HELPERCALL(Op_StrictEqualEmptyString, Js::JavascriptOperators::StrictEqualEmptyString, 0)
HELPERCALL(Op_NotStrictEqual, Js::JavascriptOperators::NotStrictEqual, 0)

HELPERCALL(Op_SwitchStringLookUp, Js::JavascriptNativeOperators::Op_SwitchStringLookUp, 0)

HELPERCALL(Op_HasProperty, Js::JavascriptOperators::OP_HasProperty, 0)
HELPERCALL(Op_GetProperty, Js::JavascriptOperators::OP_GetProperty, AttrCanThrow)
HELPERCALL(Op_GetInstanceScoped, Js::JavascriptOperators::OP_GetInstanceScoped, 0)
HELPERCALL(Op_StFunctionExpression, Js::JavascriptOperators::OP_StFunctionExpression, 0)
HELPERCALL(Op_InitUndeclRootLetFld, Js::JavascriptOperators::OP_InitUndeclRootLetProperty, AttrCanThrow)
HELPERCALL(Op_InitUndeclRootConstFld, Js::JavascriptOperators::OP_InitUndeclRootConstProperty, AttrCanThrow)
HELPERCALL(Op_InitUndeclConsoleLetFld, Js::JavascriptOperators::OP_InitUndeclConsoleLetProperty, AttrCanThrow)
HELPERCALL(Op_InitUndeclConsoleConstFld, Js::JavascriptOperators::OP_InitUndeclConsoleConstProperty, AttrCanThrow)
HELPERCALL(Op_InitLetFld, Js::JavascriptOperators::OP_InitLetProperty, AttrCanThrow)
HELPERCALL(Op_InitConstFld, Js::JavascriptOperators::OP_InitConstProperty, AttrCanThrow)

HELPERCALL(Op_InitClassMember, Js::JavascriptOperators::OP_InitClassMember, AttrCanThrow)
HELPERCALL(Op_InitClassMemberSet, Js::JavascriptOperators::OP_InitClassMemberSet, AttrCanThrow)
HELPERCALL(Op_InitClassMemberSetComputedName, Js::JavascriptOperators::OP_InitClassMemberSetComputedName, AttrCanThrow)
HELPERCALL(Op_InitClassMemberGet, Js::JavascriptOperators::OP_InitClassMemberGet, AttrCanThrow)
HELPERCALL(Op_InitClassMemberGetComputedName, Js::JavascriptOperators::OP_InitClassMemberGetComputedName, AttrCanThrow)
HELPERCALL(Op_InitClassMemberComputedName, Js::JavascriptOperators::OP_InitClassMemberComputedName, AttrCanThrow)

HELPERCALL(Op_InitFuncScoped, Js::JavascriptOperators::OP_InitFuncScoped, AttrCanThrow)
HELPERCALL(Op_DeleteProperty, Js::JavascriptOperators::OP_DeleteProperty, AttrCanThrow)
HELPERCALL(Op_DeleteRootProperty, Js::JavascriptOperators::OP_DeleteRootProperty, AttrCanThrow)
HELPERCALL(Op_DeletePropertyScoped, Js::JavascriptOperators::OP_DeletePropertyScoped, AttrCanThrow)
HELPERCALL(Op_GetElementI, Js::JavascriptOperators::OP_GetElementI_JIT, AttrCanThrow)
HELPERCALL(Op_GetElementI_ExpectingNativeFloatArray, Js::JavascriptOperators::OP_GetElementI_JIT_ExpectingNativeFloatArray, AttrCanThrow)
HELPERCALL(Op_GetElementI_ExpectingVarArray, Js::JavascriptOperators::OP_GetElementI_JIT_ExpectingVarArray, AttrCanThrow)
HELPERCALL(Op_GetElementI_UInt32, Js::JavascriptOperators::OP_GetElementI_UInt32, AttrCanThrow)
HELPERCALL(Op_GetElementI_UInt32_ExpectingNativeFloatArray, Js::JavascriptOperators::OP_GetElementI_UInt32_ExpectingNativeFloatArray, AttrCanThrow)
HELPERCALL(Op_GetElementI_UInt32_ExpectingVarArray, Js::JavascriptOperators::OP_GetElementI_UInt32_ExpectingVarArray, AttrCanThrow)
HELPERCALL(Op_GetElementI_Int32, Js::JavascriptOperators::OP_GetElementI_Int32, AttrCanThrow)
HELPERCALL(Op_GetElementI_Int32_ExpectingNativeFloatArray, Js::JavascriptOperators::OP_GetElementI_Int32_ExpectingNativeFloatArray, AttrCanThrow)
HELPERCALL(Op_GetElementI_Int32_ExpectingVarArray, Js::JavascriptOperators::OP_GetElementI_Int32_ExpectingVarArray, AttrCanThrow)
HELPERCALL(Op_GetNativeIntElementI, Js::JavascriptOperators::OP_GetNativeIntElementI, AttrCanThrow)
HELPERCALL(Op_GetNativeFloatElementI, Js::JavascriptOperators::OP_GetNativeFloatElementI, AttrCanThrow)
HELPERCALL(Op_GetNativeIntElementI_Int32, Js::JavascriptOperators::OP_GetNativeIntElementI_Int32, AttrCanThrow)
HELPERCALL(Op_GetNativeFloatElementI_Int32, Js::JavascriptOperators::OP_GetNativeFloatElementI_Int32, AttrCanThrow)
HELPERCALL(Op_GetNativeIntElementI_UInt32, Js::JavascriptOperators::OP_GetNativeIntElementI_UInt32, AttrCanThrow)
HELPERCALL(Op_GetNativeFloatElementI_UInt32, Js::JavascriptOperators::OP_GetNativeFloatElementI_UInt32, AttrCanThrow)
HELPERCALL(Op_GetMethodElement, Js::JavascriptOperators::OP_GetMethodElement, AttrCanThrow)
HELPERCALL(Op_GetMethodElement_UInt32, Js::JavascriptOperators::OP_GetMethodElement_UInt32, AttrCanThrow)
HELPERCALL(Op_GetMethodElement_Int32, Js::JavascriptOperators::OP_GetMethodElement_Int32, AttrCanThrow)
HELPERCALL(Op_SetElementI, Js::JavascriptOperators::OP_SetElementI_JIT, AttrCanThrow)
HELPERCALL(Op_SetElementI_UInt32, Js::JavascriptOperators::OP_SetElementI_UInt32, AttrCanThrow)
HELPERCALL(Op_SetElementI_Int32, Js::JavascriptOperators::OP_SetElementI_Int32, AttrCanThrow)
HELPERCALL(Op_SetNativeIntElementI, Js::JavascriptOperators::OP_SetNativeIntElementI, AttrCanThrow)
HELPERCALL(Op_SetNativeFloatElementI, Js::JavascriptOperators::OP_SetNativeFloatElementI, AttrCanThrow)
HELPERCALL(Op_SetNativeIntElementI_Int32, Js::JavascriptOperators::OP_SetNativeIntElementI_Int32, AttrCanThrow)
HELPERCALL(Op_SetNativeFloatElementI_Int32, Js::JavascriptOperators::OP_SetNativeFloatElementI_Int32, AttrCanThrow)
HELPERCALL(Op_SetNativeIntElementI_UInt32, Js::JavascriptOperators::OP_SetNativeIntElementI_UInt32, AttrCanThrow)
HELPERCALL(Op_SetNativeFloatElementI_UInt32, Js::JavascriptOperators::OP_SetNativeFloatElementI_UInt32, AttrCanThrow)
HELPERCALL(ScrArr_SetNativeIntElementC, Js::JavascriptArray::OP_SetNativeIntElementC, AttrCanThrow)
HELPERCALL(ScrArr_SetNativeFloatElementC, Js::JavascriptArray::OP_SetNativeFloatElementC, AttrCanThrow)
HELPERCALL(Op_DeleteElementI, Js::JavascriptOperators::OP_DeleteElementI, AttrCanThrow)
HELPERCALL(Op_DeleteElementI_UInt32, Js::JavascriptOperators::OP_DeleteElementI_UInt32, AttrCanThrow)
HELPERCALL(Op_DeleteElementI_Int32, Js::JavascriptOperators::OP_DeleteElementI_Int32, AttrCanThrow)

HELPERCALL(Op_Memset, Js::JavascriptOperators::OP_Memset, AttrCanThrow)
HELPERCALL(Op_Memcopy, Js::JavascriptOperators::OP_Memcopy, AttrCanThrow)

HELPERCALL(Op_PatchGetValue, ((Js::Var (*)(Js::FunctionBody *const, Js::InlineCache *const, const Js::InlineCacheIndex, Js::Var, Js::PropertyId))Js::JavascriptOperators::PatchGetValue<true, Js::InlineCache>), AttrCanThrow)
HELPERCALL(Op_PatchGetValueWithThisPtr, ((Js::Var(*)(Js::FunctionBody *const, Js::InlineCache *const, const Js::InlineCacheIndex, Js::Var, Js::PropertyId, Js::Var))Js::JavascriptOperators::PatchGetValueWithThisPtr<true, Js::InlineCache>), AttrCanThrow)
HELPERCALL(Op_PatchGetValueForTypeOf, ((Js::Var(*)(Js::FunctionBody *const, Js::InlineCache *const, const Js::InlineCacheIndex, Js::Var, Js::PropertyId))Js::JavascriptOperators::PatchGetValueForTypeOf<true, Js::InlineCache>), AttrCanThrow)
HELPERCALL(Op_PatchGetValuePolymorphic, ((Js::Var (*)(Js::FunctionBody *const, Js::PolymorphicInlineCache *const, const Js::InlineCacheIndex, Js::Var, Js::PropertyId))Js::JavascriptOperators::PatchGetValue<true, Js::PolymorphicInlineCache>), AttrCanThrow)
HELPERCALL(Op_PatchGetValuePolymorphicWithThisPtr, ((Js::Var(*)(Js::FunctionBody *const, Js::PolymorphicInlineCache *const, const Js::InlineCacheIndex, Js::Var, Js::PropertyId, Js::Var))Js::JavascriptOperators::PatchGetValueWithThisPtr<true, Js::PolymorphicInlineCache>), AttrCanThrow)
HELPERCALL(Op_PatchGetValuePolymorphicForTypeOf, ((Js::Var(*)(Js::FunctionBody *const, Js::PolymorphicInlineCache *const, const Js::InlineCacheIndex, Js::Var, Js::PropertyId))Js::JavascriptOperators::PatchGetValueForTypeOf<true, Js::PolymorphicInlineCache>), AttrCanThrow)

HELPERCALL(Op_PatchGetRootValue, ((Js::Var (*)(Js::FunctionBody *const, Js::InlineCache *const, const Js::InlineCacheIndex, Js::DynamicObject*, Js::PropertyId))Js::JavascriptOperators::PatchGetRootValue<true, Js::InlineCache>), AttrCanThrow)
HELPERCALL(Op_PatchGetRootValuePolymorphic, ((Js::Var (*)(Js::FunctionBody *const, Js::PolymorphicInlineCache *const, const Js::InlineCacheIndex, Js::DynamicObject*, Js::PropertyId))Js::JavascriptOperators::PatchGetRootValue<true, Js::PolymorphicInlineCache>), AttrCanThrow)

HELPERCALL(Op_PatchGetRootValueForTypeOf, ((Js::Var(*)(Js::FunctionBody *const, Js::InlineCache *const, const Js::InlineCacheIndex, Js::DynamicObject*, Js::PropertyId))Js::JavascriptOperators::PatchGetRootValueForTypeOf<true, Js::InlineCache>), AttrCanThrow)
HELPERCALL(Op_PatchGetRootValuePolymorphicForTypeOf, ((Js::Var(*)(Js::FunctionBody *const, Js::PolymorphicInlineCache *const, const Js::InlineCacheIndex, Js::DynamicObject*, Js::PropertyId))Js::JavascriptOperators::PatchGetRootValueForTypeOf<true, Js::PolymorphicInlineCache>), AttrCanThrow)

HELPERCALL(Op_PatchGetPropertyScoped, ((Js::Var (*)(Js::FunctionBody *const, Js::InlineCache *const, const Js::InlineCacheIndex, Js::FrameDisplay*, Js::PropertyId, Js::Var))Js::JavascriptOperators::PatchGetPropertyScoped<true, Js::InlineCache>), AttrCanThrow)
HELPERCALL(Op_PatchGetPropertyForTypeOfScoped, ((Js::Var(*)(Js::FunctionBody *const, Js::InlineCache *const, const Js::InlineCacheIndex, Js::FrameDisplay*, Js::PropertyId, Js::Var))Js::JavascriptOperators::PatchGetPropertyForTypeOfScoped<true, Js::InlineCache>), AttrCanThrow)

HELPERCALL(Op_PatchInitValue, ((void (*)(Js::FunctionBody *const, Js::InlineCache *const, const Js::InlineCacheIndex, Js::RecyclableObject*, Js::PropertyId, Js::Var))Js::JavascriptOperators::PatchInitValue<true, Js::InlineCache>), AttrCanThrow)
HELPERCALL(Op_PatchInitValuePolymorphic, ((void (*)(Js::FunctionBody *const, Js::PolymorphicInlineCache *const, const Js::InlineCacheIndex, Js::RecyclableObject*, Js::PropertyId, Js::Var))Js::JavascriptOperators::PatchInitValue<true, Js::PolymorphicInlineCache>), AttrCanThrow)

HELPERCALL(Op_PatchPutValue, ((void (*)(Js::FunctionBody *const, Js::InlineCache *const, const Js::InlineCacheIndex, Js::Var, Js::PropertyId, Js::Var, Js::PropertyOperationFlags))Js::JavascriptOperators::PatchPutValue<true, Js::InlineCache>), AttrCanThrow)
HELPERCALL(Op_PatchPutValueWithThisPtr, ((void (*)(Js::FunctionBody *const, Js::InlineCache *const, const Js::InlineCacheIndex, Js::Var, Js::PropertyId, Js::Var, Js::Var, Js::PropertyOperationFlags))Js::JavascriptOperators::PatchPutValueWithThisPtr<true, Js::InlineCache>), AttrCanThrow)
HELPERCALL(Op_PatchPutValuePolymorphic, ((void (*)(Js::FunctionBody *const, Js::PolymorphicInlineCache *const, const Js::InlineCacheIndex, Js::Var, Js::PropertyId, Js::Var, Js::PropertyOperationFlags))Js::JavascriptOperators::PatchPutValue<true, Js::PolymorphicInlineCache>), AttrCanThrow)
HELPERCALL(Op_PatchPutValueWithThisPtrPolymorphic, ((void (*)(Js::FunctionBody *const, Js::PolymorphicInlineCache *const, const Js::InlineCacheIndex, Js::Var, Js::PropertyId, Js::Var, Js::Var, Js::PropertyOperationFlags))Js::JavascriptOperators::PatchPutValueWithThisPtr<true, Js::PolymorphicInlineCache>), AttrCanThrow)
HELPERCALL(Op_PatchPutRootValue, ((void (*)(Js::FunctionBody *const, Js::InlineCache *const, const Js::InlineCacheIndex, Js::Var, Js::PropertyId, Js::Var, Js::PropertyOperationFlags))Js::JavascriptOperators::PatchPutRootValue<true, Js::InlineCache>), AttrCanThrow)
HELPERCALL(Op_PatchPutRootValuePolymorphic, ((void (*)(Js::FunctionBody *const, Js::PolymorphicInlineCache *const, const Js::InlineCacheIndex, Js::Var, Js::PropertyId, Js::Var, Js::PropertyOperationFlags))Js::JavascriptOperators::PatchPutRootValue<true, Js::PolymorphicInlineCache>), AttrCanThrow)

HELPERCALL(Op_PatchPutValueNoLocalFastPath, ((void (*)(Js::FunctionBody *const, Js::InlineCache *const, const Js::InlineCacheIndex, Js::Var, Js::PropertyId, Js::Var, Js::PropertyOperationFlags))Js::JavascriptOperators::PatchPutValueNoLocalFastPath<true, Js::InlineCache>), AttrCanThrow)
HELPERCALL(Op_PatchPutValueWithThisPtrNoLocalFastPath, ((void (*)(Js::FunctionBody *const, Js::InlineCache *const, const Js::InlineCacheIndex, Js::Var, Js::PropertyId, Js::Var, Js::Var, Js::PropertyOperationFlags))Js::JavascriptOperators::PatchPutValueWithThisPtrNoLocalFastPath<true, Js::InlineCache>), AttrCanThrow)
HELPERCALL(Op_PatchPutValueNoLocalFastPathPolymorphic, ((void (*)(Js::FunctionBody *const, Js::PolymorphicInlineCache *const, const Js::InlineCacheIndex, Js::Var, Js::PropertyId, Js::Var, Js::PropertyOperationFlags))Js::JavascriptOperators::PatchPutValueNoLocalFastPath<true, Js::PolymorphicInlineCache>), AttrCanThrow)
HELPERCALL(Op_PatchPutValueWithThisPtrNoLocalFastPathPolymorphic, ((void (*)(Js::FunctionBody *const, Js::PolymorphicInlineCache *const, const Js::InlineCacheIndex, Js::Var, Js::PropertyId, Js::Var, Js::Var, Js::PropertyOperationFlags))Js::JavascriptOperators::PatchPutValueWithThisPtrNoLocalFastPath<true, Js::PolymorphicInlineCache>), AttrCanThrow)
HELPERCALL(Op_PatchPutRootValueNoLocalFastPath, ((void (*)(Js::FunctionBody *const, Js::InlineCache *const, const Js::InlineCacheIndex, Js::Var, Js::PropertyId, Js::Var, Js::PropertyOperationFlags))Js::JavascriptOperators::PatchPutRootValueNoLocalFastPath<true, Js::InlineCache>), AttrCanThrow)
HELPERCALL(Op_PatchPutRootValueNoLocalFastPathPolymorphic, ((void (*)(Js::FunctionBody *const, Js::PolymorphicInlineCache *const, const Js::InlineCacheIndex, Js::Var, Js::PropertyId, Js::Var, Js::PropertyOperationFlags))Js::JavascriptOperators::PatchPutRootValueNoLocalFastPath<true, Js::PolymorphicInlineCache>), AttrCanThrow)

HELPERCALL(Op_PatchSetPropertyScoped, ((void (*)(Js::FunctionBody *const, Js::InlineCache *const, const Js::InlineCacheIndex, Js::FrameDisplay*, Js::PropertyId, Js::Var, Js::Var, Js::PropertyOperationFlags))Js::JavascriptOperators::PatchSetPropertyScoped<true, Js::InlineCache>), AttrCanThrow)
HELPERCALL(Op_ConsolePatchSetPropertyScoped, ((void(*)(Js::FunctionBody *const, Js::InlineCache *const, const Js::InlineCacheIndex, Js::FrameDisplay*, Js::PropertyId, Js::Var, Js::Var, Js::PropertyOperationFlags))Js::JavascriptOperators::PatchSetPropertyScoped<true, Js::InlineCache>), AttrCanThrow)

HELPERCALL(Op_PatchGetMethod, ((Js::Var (*)(Js::FunctionBody *const, Js::InlineCache *const, const Js::InlineCacheIndex, Js::Var, Js::PropertyId, bool))Js::JavascriptOperators::PatchGetMethod<true, Js::InlineCache>), AttrCanThrow)
HELPERCALL(Op_PatchGetMethodPolymorphic, ((Js::Var (*)(Js::FunctionBody *const, Js::PolymorphicInlineCache *const, const Js::InlineCacheIndex, Js::Var, Js::PropertyId, bool))Js::JavascriptOperators::PatchGetMethod<true, Js::PolymorphicInlineCache>), AttrCanThrow)
HELPERCALL(Op_PatchGetRootMethod, ((Js::Var (*)(Js::FunctionBody *const, Js::InlineCache *const, const Js::InlineCacheIndex, Js::Var, Js::PropertyId, bool))Js::JavascriptOperators::PatchGetRootMethod<true, Js::InlineCache>), AttrCanThrow)
HELPERCALL(Op_PatchGetRootMethodPolymorphic, ((Js::Var (*)(Js::FunctionBody *const, Js::PolymorphicInlineCache *const, const Js::InlineCacheIndex, Js::Var, Js::PropertyId, bool))Js::JavascriptOperators::PatchGetRootMethod<true, Js::PolymorphicInlineCache>), AttrCanThrow)
HELPERCALL(Op_ScopedGetMethod, ((Js::Var (*)(Js::FunctionBody *const, Js::InlineCache *const, const Js::InlineCacheIndex, Js::Var, Js::PropertyId, bool))Js::JavascriptOperators::PatchScopedGetMethod<true, Js::InlineCache>), AttrCanThrow)
HELPERCALL(Op_ScopedGetMethodPolymorphic, ((Js::Var (*)(Js::FunctionBody *const, Js::PolymorphicInlineCache *const, const Js::InlineCacheIndex, Js::Var, Js::PropertyId, bool))Js::JavascriptOperators::PatchScopedGetMethod<true, Js::PolymorphicInlineCache>), AttrCanThrow)

HELPERCALL(CheckIfTypeIsEquivalent, Js::JavascriptOperators::CheckIfTypeIsEquivalent, 0)

HELPERCALL(Op_Delete, Js::JavascriptOperators::Delete, AttrCanThrow)
HELPERCALL(OP_InitSetter, Js::JavascriptOperators::OP_InitSetter, AttrCanThrow)
HELPERCALL(OP_InitElemSetter, Js::JavascriptOperators::OP_InitElemSetter, AttrCanThrow)
HELPERCALL(OP_InitGetter, Js::JavascriptOperators::OP_InitGetter, AttrCanThrow)
HELPERCALL(OP_InitElemGetter, Js::JavascriptOperators::OP_InitElemGetter, AttrCanThrow)
HELPERCALL(OP_InitComputedProperty, Js::JavascriptOperators::OP_InitComputedProperty, AttrCanThrow)
HELPERCALL(OP_InitProto, Js::JavascriptOperators::OP_InitProto, AttrCanThrow)

HELPERCALL(Op_OP_GetForInEnumerator, Js::JavascriptOperators::OP_GetForInEnumerator, 0)
HELPERCALL(Op_OP_ReleaseForInEnumerator, Js::JavascriptOperators::OP_ReleaseForInEnumerator, 0)
HELPERCALL(Op_OP_BrOnEmpty, Js::JavascriptOperators::OP_BrOnEmpty, 0)

HELPERCALL(Op_OP_BrFncEqApply, Js::JavascriptOperators::OP_BrFncEqApply, 0)
HELPERCALL(Op_OP_BrFncNeqApply, Js::JavascriptOperators::OP_BrFncNeqApply, 0)
HELPERCALL(Op_OP_ApplyArgs, Js::JavascriptOperators::OP_ApplyArgs, 0)

HELPERCALL(Conv_ToBoolean, Js::JavascriptConversion::ToBoolean, 0)
HELPERCALL(ScrArr_ProfiledNewInstance, Js::JavascriptArray::ProfiledNewInstance, 0)
HELPERCALL(ScrArr_ProfiledNewInstanceNoArg, Js::JavascriptArray::ProfiledNewInstanceNoArg, 0)
HELPERCALL(ScrArr_OP_NewScArray, Js::JavascriptArray::OP_NewScArray, 0)
HELPERCALL(ScrArr_ProfiledNewScArray, Js::JavascriptArray::ProfiledNewScArray, 0)
HELPERCALL(ScrArr_OP_NewScArrayWithElements, Js::JavascriptArray::OP_NewScArrayWithElements, 0)
HELPERCALL(ScrArr_OP_NewScArrayWithMissingValues, Js::JavascriptArray::OP_NewScArrayWithMissingValues, 0)
HELPERCALL(ScrArr_OP_NewScIntArray, Js::JavascriptArray::OP_NewScIntArray, 0)
HELPERCALL(ScrArr_ProfiledNewScIntArray, Js::JavascriptArray::ProfiledNewScIntArray, 0)
HELPERCALL(ScrArr_OP_NewScFltArray, Js::JavascriptArray::OP_NewScFltArray, 0)
HELPERCALL(ScrArr_ProfiledNewScFltArray, Js::JavascriptArray::ProfiledNewScFltArray, 0)
HELPERCALL(ArraySegmentVars, Js::JavascriptOperators::AddVarsToArraySegment, 0)
HELPERCALL(IntArr_ToNativeFloatArray, Js::JavascriptNativeIntArray::ToNativeFloatArray, AttrCanThrow)
HELPERCALL(IntArr_ToVarArray, Js::JavascriptNativeIntArray::ToVarArray, AttrCanThrow)
HELPERCALL(FloatArr_ToVarArray, Js::JavascriptNativeFloatArray::ToVarArray, AttrCanThrow)

HELPERCALL(Array_Jit_GetArrayHeadSegmentForArrayOrObjectWithArray, Js::JavascriptArray::Jit_GetArrayHeadSegmentForArrayOrObjectWithArray, 0)
HELPERCALL(Array_Jit_GetArrayHeadSegmentLength, Js::JavascriptArray::Jit_GetArrayHeadSegmentLength, 0)
HELPERCALL(Array_Jit_OperationInvalidatedArrayHeadSegment, Js::JavascriptArray::Jit_OperationInvalidatedArrayHeadSegment, 0)
HELPERCALL(Array_Jit_GetArrayLength, Js::JavascriptArray::Jit_GetArrayLength, 0)
HELPERCALL(Array_Jit_OperationInvalidatedArrayLength, Js::JavascriptArray::Jit_OperationInvalidatedArrayLength, 0)
HELPERCALL(Array_Jit_GetArrayFlagsForArrayOrObjectWithArray, Js::JavascriptArray::Jit_GetArrayFlagsForArrayOrObjectWithArray, 0)
HELPERCALL(Array_Jit_OperationCreatedFirstMissingValue, Js::JavascriptArray::Jit_OperationCreatedFirstMissingValue, 0)

HELPERCALL(AllocMemForConcatStringMulti, (void (*)(size_t, Recycler*))Js::JavascriptOperators::JitRecyclerAlloc<Js::ConcatStringMulti>, 0)
HELPERCALL(AllocMemForScObject, (void (*)(size_t, Recycler*))Js::JavascriptOperators::JitRecyclerAlloc<Js::DynamicObject>, 0)
HELPERCALL(AllocMemForJavascriptArray, (void (*)(size_t, Recycler*))Js::JavascriptOperators::JitRecyclerAlloc<Js::JavascriptArray>, 0)
HELPERCALL(AllocMemForJavascriptNativeIntArray, (void (*)(size_t, Recycler*))Js::JavascriptOperators::JitRecyclerAlloc<Js::JavascriptNativeIntArray>, 0)
HELPERCALL(AllocMemForJavascriptNativeFloatArray, (void (*)(size_t, Recycler*))Js::JavascriptOperators::JitRecyclerAlloc<Js::JavascriptNativeFloatArray>, 0)
HELPERCALL(AllocMemForSparseArraySegmentBase, (void (*)(size_t, Recycler*))Js::JavascriptOperators::JitRecyclerAlloc<Js::SparseArraySegmentBase>, 0)
HELPERCALL(AllocMemForFrameDisplay, (void (*)(size_t, Recycler*))Js::JavascriptOperators::JitRecyclerAlloc<Js::FrameDisplay>, 0)
HELPERCALL(AllocMemForVarArray, Js::JavascriptOperators::AllocMemForVarArray, 0)
HELPERCALL(AllocMemForJavascriptRegExp, (void (*)(size_t, Recycler*))Js::JavascriptOperators::JitRecyclerAlloc<Js::JavascriptRegExp>, 0)
HELPERCALL(NewJavascriptObjectNoArg, Js::JavascriptOperators::NewJavascriptObjectNoArg, 0)
HELPERCALL(NewJavascriptArrayNoArg, Js::JavascriptOperators::NewJavascriptArrayNoArg, 0)
HELPERCALL(NewScObjectNoArg, Js::JavascriptOperators::NewScObjectNoArg, 0)
HELPERCALL(NewScObjectNoCtor, Js::JavascriptOperators::NewScObjectNoCtor, 0)
HELPERCALL(NewScObjectNoCtorFull, Js::JavascriptOperators::NewScObjectNoCtorFull, 0)
HELPERCALL(NewScObjectNoArgNoCtorFull, Js::JavascriptOperators::NewScObjectNoArgNoCtorFull, 0)
HELPERCALL(NewScObjectNoArgNoCtor, Js::JavascriptOperators::NewScObjectNoArgNoCtor, 0)
HELPERCALL(UpdateNewScObjectCache, Js::JavascriptOperators::UpdateNewScObjectCache, 0)
HELPERCALL(EnsureObjectLiteralType, Js::JavascriptOperators::EnsureObjectLiteralType, 0)

HELPERCALL(OP_InitClass, Js::JavascriptOperators::OP_InitClass, AttrCanThrow)

HELPERCALL(OP_Freeze, Js::JavascriptOperators::OP_Freeze, AttrCanThrow)
HELPERCALL(OP_ClearAttributes, Js::JavascriptOperators::OP_ClearAttributes, AttrCanThrow)

HELPERCALL(OP_CmEq_A, Js::JavascriptOperators::OP_CmEq_A, 0)
HELPERCALL(OP_CmNeq_A, Js::JavascriptOperators::OP_CmNeq_A, 0)
HELPERCALL(OP_CmSrEq_A, Js::JavascriptOperators::OP_CmSrEq_A, 0)
HELPERCALL(OP_CmSrEq_String, Js::JavascriptOperators::OP_CmSrEq_String, 0)
HELPERCALL(OP_CmSrEq_EmptyString, Js::JavascriptOperators::OP_CmSrEq_EmptyString, 0)
HELPERCALL(OP_CmSrNeq_A, Js::JavascriptOperators::OP_CmSrNeq_A, 0)
HELPERCALL(OP_CmLt_A, Js::JavascriptOperators::OP_CmLt_A, AttrCanThrow)
HELPERCALL(OP_CmLe_A, Js::JavascriptOperators::OP_CmLe_A, AttrCanThrow)
HELPERCALL(OP_CmGt_A, Js::JavascriptOperators::OP_CmGt_A, AttrCanThrow)
HELPERCALL(OP_CmGe_A, Js::JavascriptOperators::OP_CmGe_A, AttrCanThrow)

HELPERCALL(Conv_ToUInt32_Full, Js::JavascriptConversion::ToUInt32_Full, AttrCanThrow)
HELPERCALL(Conv_ToUInt32, (uint32 (*)(Js::Var, Js::ScriptContext *))Js::JavascriptConversion::ToUInt32, AttrCanThrow)
#ifdef _M_IX86
HELPERCALL(Op_Int32ToAtom, Js::JavascriptOperators::Int32ToVar, 0)
HELPERCALL(Op_Int32ToAtomInPlace, Js::JavascriptOperators::Int32ToVarInPlace, 0)
HELPERCALL(Op_UInt32ToAtom, Js::JavascriptOperators::UInt32ToVar, 0)
HELPERCALL(Op_UInt32ToAtomInPlace, Js::JavascriptOperators::UInt32ToVarInPlace, 0)
#endif
#if !FLOATVAR
HELPERCALL(AllocUninitializedNumber, Js::JavascriptOperators::AllocUninitializedNumber, 0)
#endif

// SIMD_JS
HELPERCALL(AllocUninitializedSimdF4, Js::JavascriptSIMDFloat32x4::AllocUninitialized, 0)
HELPERCALL(AllocUninitializedSimdI4, Js::JavascriptSIMDInt32x4::AllocUninitialized, 0)

HELPERCALL(Op_TryCatch, nullptr, 0)
HELPERCALL(Op_TryFinally, nullptr, AttrCanThrow)
#if _M_X64
HELPERCALL(Op_ReturnFromCallWithFakeFrame, amd64_ReturnFromCallWithFakeFrame, 0)
#endif
HELPERCALL(Op_Throw, Js::JavascriptExceptionOperators::OP_Throw, AttrCanThrow)
HELPERCALL(Op_RuntimeTypeError, Js::JavascriptExceptionOperators::OP_RuntimeTypeError, AttrCanThrow)
HELPERCALL(Op_RuntimeRangeError, Js::JavascriptExceptionOperators::OP_RuntimeRangeError, AttrCanThrow)
HELPERCALL(Op_RuntimeReferenceError, Js::JavascriptExceptionOperators::OP_RuntimeReferenceError, AttrCanThrow)
HELPERCALL(Op_OutOfMemoryError, Js::Throw::OutOfMemory, AttrCanThrow)
HELPERCALL(Op_FatalInternalError, Js::Throw::FatalInternalError, AttrCanThrow)

HELPERCALL(Op_InvokePut, Js::JavascriptOperators::OP_InvokePut, 0)
#if ENABLE_REGEX_CONFIG_OPTIONS
HELPERCALL(ScrRegEx_OP_NewRegEx, Js::JavascriptRegExp::OP_NewRegEx, 0)
#endif
HELPERCALL(ProbeCurrentStack, ThreadContext::ProbeCurrentStack, 0)
HELPERCALL(ProbeCurrentStack2, ThreadContext::ProbeCurrentStack2, 0)

HELPERCALL(AdjustSlots, Js::DynamicTypeHandler::AdjustSlots_Jit, 0)
HELPERCALL(InvalidateProtoCaches, Js::JavascriptOperators::OP_InvalidateProtoCaches, 0)
HELPERCALL(CheckProtoHasNonWritable, Js::JavascriptOperators::DoCheckIfPrototypeChainHasOnlyWritableDataProperties, 0)

HELPERCALL(GetStringForChar, (Js::JavascriptString * (*)(Js::CharStringCache *, wchar_t))&Js::CharStringCache::GetStringForChar, 0)
HELPERCALL(GetStringForCharCodePoint, (Js::JavascriptString * (*)(Js::CharStringCache *, codepoint_t))&Js::CharStringCache::GetStringForCharCodePoint, 0)

HELPERCALL(ProfiledLdElem, Js::ProfilingHelpers::ProfiledLdElem, 0)
HELPERCALL(ProfiledStElem_DefaultFlags, Js::ProfilingHelpers::ProfiledStElem_DefaultFlags, 0)
HELPERCALL(ProfiledStElem, Js::ProfilingHelpers::ProfiledStElem, 0)
HELPERCALL(ProfiledNewScArray, Js::ProfilingHelpers::ProfiledNewScArray, 0)
HELPERCALL(ProfiledNewScObjArray, Js::ProfilingHelpers::ProfiledNewScObjArray_Jit, 0)
HELPERCALL(ProfiledNewScObjArraySpread, Js::ProfilingHelpers::ProfiledNewScObjArraySpread_Jit, 0)
HELPERCALL(ProfileLdSlot, Js::ProfilingHelpers::ProfileLdSlot, 0)
HELPERCALL(ProfiledLdFld, Js::ProfilingHelpers::ProfiledLdFld_Jit, 0)
HELPERCALL(ProfiledLdSuperFld, Js::ProfilingHelpers::ProfiledLdSuperFld_Jit, 0)
HELPERCALL(ProfiledLdFldForTypeOf, Js::ProfilingHelpers::ProfiledLdFldForTypeOf_Jit, 0)
HELPERCALL(ProfiledLdRootFldForTypeOf, Js::ProfilingHelpers::ProfiledLdRootFldForTypeOf_Jit, 0)
HELPERCALL(ProfiledLdFld_CallApplyTarget, Js::ProfilingHelpers::ProfiledLdFld_CallApplyTarget_Jit, 0)
HELPERCALL(ProfiledLdMethodFld, Js::ProfilingHelpers::ProfiledLdMethodFld_Jit, 0)
HELPERCALL(ProfiledLdRootFld, Js::ProfilingHelpers::ProfiledLdRootFld_Jit, 0)
HELPERCALL(ProfiledLdRootMethodFld, Js::ProfilingHelpers::ProfiledLdRootMethodFld_Jit, 0)
HELPERCALL(ProfiledStFld, Js::ProfilingHelpers::ProfiledStFld_Jit, 0)
HELPERCALL(ProfiledStSuperFld, Js::ProfilingHelpers::ProfiledStSuperFld_Jit, 0)
HELPERCALL(ProfiledStFld_Strict, Js::ProfilingHelpers::ProfiledStFld_Strict_Jit, 0)
HELPERCALL(ProfiledStRootFld, Js::ProfilingHelpers::ProfiledStRootFld_Jit, 0)
HELPERCALL(ProfiledStRootFld_Strict, Js::ProfilingHelpers::ProfiledStRootFld_Strict_Jit, 0)
HELPERCALL(ProfiledInitFld, Js::ProfilingHelpers::ProfiledInitFld_Jit, 0)

HELPERCALL(TransitionFromSimpleJit, NativeCodeGenerator::Jit_TransitionFromSimpleJit, 0)
HELPERCALL(SimpleProfileCall_DefaultInlineCacheIndex, Js::SimpleJitHelpers::ProfileCall_DefaultInlineCacheIndex, 0)
HELPERCALL(SimpleProfileCall, Js::SimpleJitHelpers::ProfileCall, 0)
HELPERCALL(SimpleProfileReturnTypeCall, Js::SimpleJitHelpers::ProfileReturnTypeCall, 0)
HELPERCALL(SimpleProfiledLdLen, Js::SimpleJitHelpers::ProfiledLdLen_A, AttrCanThrow) //Can throw because it mirrors OP_GetProperty
HELPERCALL(SimpleProfiledStrictLdThis, Js::SimpleJitHelpers::ProfiledStrictLdThis, 0)
HELPERCALL(SimpleProfiledLdThis, Js::SimpleJitHelpers::ProfiledLdThis, 0)
HELPERCALL(SimpleProfiledSwitch, Js::SimpleJitHelpers::ProfiledSwitch, 0)
HELPERCALL(SimpleProfiledDivide, Js::SimpleJitHelpers::ProfiledDivide, 0)
HELPERCALL(SimpleProfiledRemainder, Js::SimpleJitHelpers::ProfiledRemainder, 0)
HELPERCALL(SimpleStoreArrayHelper, Js::SimpleJitHelpers::StoreArrayHelper, 0)
HELPERCALL(SimpleStoreArraySegHelper, Js::SimpleJitHelpers::StoreArraySegHelper, 0)
HELPERCALL(SimpleProfileParameters, Js::SimpleJitHelpers::ProfileParameters, 0)
HELPERCALL(SimpleCleanImplicitCallFlags, Js::SimpleJitHelpers::CleanImplicitCallFlags, 0)
HELPERCALL(SimpleGetScheduledEntryPoint, Js::SimpleJitHelpers::GetScheduledEntryPoint, 0)
HELPERCALL(SimpleIsLoopCodeGenDone, Js::SimpleJitHelpers::IsLoopCodeGenDone, 0)
HELPERCALL(SimpleRecordLoopImplicitCallFlags, Js::SimpleJitHelpers::RecordLoopImplicitCallFlags, 0)

HELPERCALL(ScriptAbort, Js::JavascriptOperators::ScriptAbort, AttrCanThrow)

HELPERCALL(NoSaveRegistersBailOutForElidedYield, BailOutRecord::BailOutForElidedYield, 0)

// We don't want these functions to be valid iCall targets because they can be used to disclose stack addresses
//   which CFG cannot defend against. Instead, return these addresses in GetNonTableMethodAddress
HELPERCALL(SaveAllRegistersAndBailOut, nullptr, 0)
HELPERCALL(SaveAllRegistersAndBranchBailOut, nullptr, 0)
#ifdef _M_IX86
HELPERCALL(SaveAllRegistersNoSse2AndBailOut, nullptr, 0)
HELPERCALL(SaveAllRegistersNoSse2AndBranchBailOut, nullptr, 0)
#endif

//Helpers for inlining built-ins
HELPERCALL(Array_Concat, Js::JavascriptArray::EntryConcat, 0)
HELPERCALL(Array_IndexOf, Js::JavascriptArray::EntryIndexOf, 0)
HELPERCALL(Array_Includes, Js::JavascriptArray::EntryIncludes, 0)
HELPERCALL(Array_Join, Js::JavascriptArray::EntryJoin, 0)
HELPERCALL(Array_LastIndexOf, Js::JavascriptArray::EntryLastIndexOf, 0)
HELPERCALL(Array_VarPush, Js::JavascriptArray::Push, 0)
HELPERCALL(Array_NativeIntPush, Js::JavascriptNativeIntArray::Push, 0)
HELPERCALL(Array_NativeFloatPush, Js::JavascriptNativeFloatArray::Push, 0)
HELPERCALL(Array_VarPop, Js::JavascriptArray::Pop, 0)
HELPERCALL(Array_NativePopWithNoDst, Js::JavascriptNativeArray::PopWithNoDst, 0)
HELPERCALL(Array_NativeIntPop, Js::JavascriptNativeIntArray::Pop, 0)
HELPERCALL(Array_NativeFloatPop, Js::JavascriptNativeFloatArray::Pop, 0)
HELPERCALL(Array_Reverse, Js::JavascriptArray::EntryReverse, 0)
HELPERCALL(Array_Shift, Js::JavascriptArray::EntryShift, 0)
HELPERCALL(Array_Slice, Js::JavascriptArray::EntrySlice, 0)
HELPERCALL(Array_Splice, Js::JavascriptArray::EntrySplice, 0)
HELPERCALL(Array_Unshift, Js::JavascriptArray::EntryUnshift, 0)

HELPERCALL(String_Concat, Js::JavascriptString::EntryConcat, 0)
HELPERCALL(String_CharCodeAt, Js::JavascriptString::EntryCharCodeAt, 0)
HELPERCALL(String_CharAt, Js::JavascriptString::EntryCharAt, 0)
HELPERCALL(String_FromCharCode, Js::JavascriptString::EntryFromCharCode, 0)
HELPERCALL(String_FromCodePoint, Js::JavascriptString::EntryFromCodePoint, 0)
HELPERCALL(String_IndexOf, Js::JavascriptString::EntryIndexOf, 0)
HELPERCALL(String_LastIndexOf, Js::JavascriptString::EntryLastIndexOf, 0)
HELPERCALL(String_Link, Js::JavascriptString::EntryLink, 0)
HELPERCALL(String_LocaleCompare, Js::JavascriptString::EntryLocaleCompare, 0)
HELPERCALL(String_Match, Js::JavascriptString::EntryMatch, 0)
HELPERCALL(String_Replace, Js::JavascriptString::EntryReplace, 0)
HELPERCALL(String_Search, Js::JavascriptString::EntrySearch, 0)
HELPERCALL(String_Slice, Js::JavascriptString::EntrySlice, 0)
HELPERCALL(String_Split, Js::JavascriptString::EntrySplit, 0)
HELPERCALL(String_Substr, Js::JavascriptString::EntrySubstr, 0)
HELPERCALL(String_Substring, Js::JavascriptString::EntrySubstring, 0)
HELPERCALL(String_ToLocaleLowerCase, Js::JavascriptString::EntryToLocaleLowerCase, 0)
HELPERCALL(String_ToLocaleUpperCase, Js::JavascriptString::EntryToLocaleUpperCase, 0)
HELPERCALL(String_ToLowerCase, Js::JavascriptString::EntryToLowerCase, 0)
HELPERCALL(String_ToUpperCase, Js::JavascriptString::EntryToUpperCase, 0)
HELPERCALL(String_Trim, Js::JavascriptString::EntryTrim, 0)
HELPERCALL(String_TrimLeft, Js::JavascriptString::EntryTrimLeft, 0)
HELPERCALL(String_TrimRight, Js::JavascriptString::EntryTrimRight, 0)
HELPERCALL(String_GetSz, Js::JavascriptString::GetSzHelper, 0)
HELPERCALL(GlobalObject_ParseInt, Js::GlobalObject::EntryParseInt, 0)

HELPERCALL(RegExp_SplitResultUsed, Js::RegexHelper::RegexSplitResultUsed, 0)
HELPERCALL(RegExp_SplitResultUsedAndMayBeTemp, Js::RegexHelper::RegexSplitResultUsedAndMayBeTemp, 0)
HELPERCALL(RegExp_SplitResultNotUsed, Js::RegexHelper::RegexSplitResultNotUsed, 0)
HELPERCALL(RegExp_MatchResultUsed, Js::RegexHelper::RegexMatchResultUsed, 0)
HELPERCALL(RegExp_MatchResultUsedAndMayBeTemp, Js::RegexHelper::RegexMatchResultUsedAndMayBeTemp, 0)
HELPERCALL(RegExp_MatchResultNotUsed, Js::RegexHelper::RegexMatchResultNotUsed, 0)
HELPERCALL(RegExp_Exec, Js::JavascriptRegExp::EntryExec, 0)
HELPERCALL(RegExp_ExecResultUsed, Js::RegexHelper::RegexExecResultUsed, 0)
HELPERCALL(RegExp_ExecResultUsedAndMayBeTemp, Js::RegexHelper::RegexExecResultUsedAndMayBeTemp, 0)
HELPERCALL(RegExp_ExecResultNotUsed, Js::RegexHelper::RegexExecResultNotUsed, 0)
HELPERCALL(RegExp_ReplaceStringResultUsed, Js::RegexHelper::RegexReplaceResultUsed, 0)
HELPERCALL(RegExp_ReplaceStringResultNotUsed, Js::RegexHelper::RegexReplaceResultNotUsed, 0)

HELPERCALL(Uint8ClampedArraySetItem, (BOOL (*)(Js::Uint8ClampedArray * arr, uint32 index, Js::Var value))&Js::Uint8ClampedArray::DirectSetItem, 0)
HELPERCALL(EnsureFunctionProxyDeferredPrototypeType, &Js::FunctionProxy::EnsureFunctionProxyDeferredPrototypeType, 0)

HELPERCALL(SpreadArrayLiteral, Js::JavascriptArray::SpreadArrayArgs, 0)
HELPERCALL(SpreadCall, Js::JavascriptFunction::EntrySpreadCall, 0)

HELPERCALL(LdSuper,             Js::JavascriptOperators::OP_LdSuper,            0)
HELPERCALL(LdSuperCtor,         Js::JavascriptOperators::OP_LdSuperCtor,        0)
HELPERCALL(ScopedLdSuper,       Js::JavascriptOperators::OP_ScopedLdSuper,      0)
HELPERCALL(ScopedLdSuperCtor,   Js::JavascriptOperators::OP_ScopedLdSuperCtor,  0)
HELPERCALL(SetHomeObj,          Js::JavascriptOperators::OP_SetHomeObj,         0)

HELPERCALL(ResumeYield,   Js::JavascriptOperators::OP_ResumeYield,   AttrCanThrow)
HELPERCALL(AsyncSpawn,    Js::JavascriptOperators::OP_AsyncSpawn,    AttrCanThrow)

#include "ExternalHelperMethodList.h"

#if !FLOATVAR
HELPERCALL(BoxStackNumber, Js::JavascriptNumber::BoxStackNumber, 0)
#endif

HELPERCALL(GetNonzeroInt32Value_NoTaggedIntCheck, Js::JavascriptNumber::GetNonzeroInt32Value_NoTaggedIntCheck, 0)
HELPERCALL(IsNegZero, Js::JavascriptNumber::IsNegZero, 0)

HELPERCALL(DirectMath_Pow, (double(*)(double, double))Js::JavascriptNumber::DirectPow, 0)
HELPERCALL_MATH(DirectMath_Random,  (double(*)(Js::ScriptContext*))Js::JavascriptMath::Random, (double(*)(Js::ScriptContext*))Js::SSE2::JavascriptMath::Random, 0)


//
// Putting dllimport function ptr in JnHelperMethodAddresses will cause the table to be allocated in read-write memory
// as dynamic initialization is require to load these addresses.  Use nullptr instead and handle these function in GetNonTableMethodAddress().
//

HELPERCALL(MemCmp, nullptr, 0)
HELPERCALL(MemCpy, nullptr, 0)

#ifdef _M_IX86
HELPERCALL(DirectMath_Acos, nullptr, 0)
HELPERCALL(DirectMath_Asin, nullptr, 0)
HELPERCALL(DirectMath_Atan, nullptr, 0)
HELPERCALL(DirectMath_Atan2, nullptr, 0)
HELPERCALL(DirectMath_Cos, nullptr, 0)
HELPERCALL(DirectMath_Exp, nullptr, 0)
HELPERCALL(DirectMath_Log, nullptr, 0)
HELPERCALL(DirectMath_Sin, nullptr, 0)
HELPERCALL(DirectMath_Tan, nullptr, 0)
#elif defined(_M_X64)
// AMD64 regular CRT calls -- on AMD64 calling convention is already what we want -- args in XMM0, XMM1 rather than on stack which is slower.
HELPERCALL(DirectMath_Acos, nullptr, 0)
HELPERCALL(DirectMath_Asin, nullptr, 0)
HELPERCALL(DirectMath_Atan, nullptr, 0)
HELPERCALL(DirectMath_Atan2, nullptr, 0)
HELPERCALL(DirectMath_Cos, nullptr, 0)
HELPERCALL(DirectMath_Exp, nullptr, 0)
HELPERCALL(DirectMath_Log, nullptr, 0)
HELPERCALL(DirectMath_Sin, nullptr, 0)
HELPERCALL(DirectMath_Tan, nullptr, 0)
HELPERCALL(DirectMath_FloorDb, nullptr, 0)
HELPERCALL(DirectMath_FloorFlt, nullptr, 0)
HELPERCALL(DirectMath_CeilDb, nullptr, 0)
HELPERCALL(DirectMath_CeilFlt, nullptr, 0)
#elif defined(_M_ARM32_OR_ARM64)
// ARM is similar to AMD64 -- regular CRT calls as calling convention is already what we want -- args/result in VFP registers.
HELPERCALL(DirectMath_Acos, nullptr, 0)
HELPERCALL(DirectMath_Asin, nullptr, 0)
HELPERCALL(DirectMath_Atan, nullptr, 0)
HELPERCALL(DirectMath_Atan2, nullptr, 0)
HELPERCALL(DirectMath_Cos, nullptr, 0)
HELPERCALL(DirectMath_Exp, nullptr, 0)
HELPERCALL(DirectMath_Log, nullptr, 0)
HELPERCALL(DirectMath_Sin, nullptr, 0)
HELPERCALL(DirectMath_Tan, nullptr, 0)
#endif

#ifdef _CONTROL_FLOW_GUARD
HELPERCALL(GuardCheckCall, nullptr, 0)
#endif

// This is statically initialized.
#ifdef _M_IX86
HELPERCALL( CRT_chkstk, _chkstk, 0 )
#else
HELPERCALL(CRT_chkstk, __chkstk, 0)
#endif

#undef HELPERCALL_MATH
#undef HELPERCALL_FULL_OR_INPLACE_MATH

#endif
