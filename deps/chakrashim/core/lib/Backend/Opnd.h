//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

class Value;

namespace IR {


class IntConstOpnd;
class FloatConstOpnd;
class Simd128ConstOpnd;
class HelperCallOpnd;
class SymOpnd;
class PropertySymOpnd;
class RegOpnd;
class ArrayRegOpnd;
class AddrOpnd;
class IndirOpnd;
class LabelOpnd;
class MemRefOpnd;
class RegBVOpnd;

enum OpndKind : BYTE {
    OpndKindInvalid,
    OpndKindIntConst,
    OpndKindFloatConst,
    OpndKindSimd128Const,
    OpndKindHelperCall,
    OpndKindSym,
    OpndKindReg,
    OpndKindAddr,
    OpndKindIndir,
    OpndKindLabel,
    OpndKindMemRef,
    OpndKindRegBV
};

enum AddrOpndKind : BYTE {
    // The following address kinds are safe for relocatable JIT and regular
    // JIT
    AddrOpndKindConstant,
    AddrOpndKindConstantVar, // a constant var value (null or tagged int)

    // NOTE: None of the following address kinds should be generated directly
    // or you WILL break relocatable JIT code. Each kind has a helper that
    // will generate correct code for relocatable code & non-relocatable code.
    // The only exception is places where it is KNOWN that we will never
    // generate the code in relocatable JIT.

    // use LoadScriptContextOpnd
    AddrOpndKindDynamicScriptContext,
    // use LoadVTableValueOpnd
    AddrOpndKindDynamicVtable,
    // use LoadLibraryValueOpnd
    AddrOpndKindDynamicCharStringCache,
    // use appropriate helper
    AddrOpndKindDynamicMisc,
    // no profiling in dynamic JIT
    AddrOpndKindDynamicFunctionBody,
    // use LoadRuntimeInlineCacheOpnd for runtime caches,
    // in relocatable JIT polymorphic inline caches aren't generated and can
    // be referenced directly (for now)
    AddrOpndKindDynamicInlineCache,
    // no bailouts in dynamic JIT
    AddrOpndKindDynamicBailOutRecord,
    // use appropriate helper
    AddrOpndKindDynamicVar,
    AddrOpndKindDynamicType,
    AddrOpndKindDynamicTypeHandler,
    AddrOpndKindDynamicFrameDisplay,
    AddrOpndKindDynamicGuardValueRef,
    AddrOpndKindDynamicArrayCallSiteInfo,
    AddrOpndKindDynamicFunctionBodyWeakRef,
    AddrOpndKindDynamicObjectTypeRef,
    AddrOpndKindDynamicTypeCheckGuard,
    AddrOpndKindDynamicRecyclerAllocatorEndAddressRef,
    AddrOpndKindDynamicRecyclerAllocatorFreeListRef,
    AddrOpndKindDynamicBailOutKindRef,
    AddrOpndKindDynamicAuxSlotArrayRef,
    AddrOpndKindDynamicPropertySlotRef,
    AddrOpndKindDynamicFunctionEnvironmentRef,
    AddrOpndKindDynamicIsInstInlineCacheFunctionRef,
    AddrOpndKindDynamicIsInstInlineCacheTypeRef,
    AddrOpndKindDynamicIsInstInlineCacheResultRef,
    AddrOpndKindSz,
    AddrOpndKindDynamicFloatRef,
    AddrOpndKindDynamicDoubleRef,
};

///---------------------------------------------------------------------------
///
/// class Opnd
///
///     IntConstOpnd        ; int values
///     FLoatConstOpnd      ; float values
///     HelperCallOpnd      ; lib helper address (more convenient dumps than AddrOpnd)
///     SymOpnd             ; stack symbol operand (not enregistered)
///     RegOpnd             ; register operand
///     AddrOpnd            ; address or var operand (includes TaggedInt's)
///     IndirOpnd           ; indirections operand (also used for JS array references)
///     LabelOpnd           ; label operand
///     MemRefOpnd          ; direct memory reference at a given memory address.
///     RegBVOpnd           ; unsigned int bit field used to denote a bit field vector. Example: Registers to push in STM.
///
///---------------------------------------------------------------------------

class Opnd
{
protected:
    Opnd() :
        m_inUse(false),
        m_isDead(false),
        m_isValueTypeFixed(false),
        canStoreTemp(false),
        isDiagHelperCallOpnd(false),
        isPropertySymOpnd(false)
    {
#if DBG
        isFakeDst = false;
#endif
        m_kind = (OpndKind)0;
    }

    Opnd(const Opnd& oldOpnd) :
        m_type(oldOpnd.m_type),
        m_isDead(false),
        m_inUse(false),
        m_isValueTypeFixed(false),
        canStoreTemp(oldOpnd.canStoreTemp)
    {
#if DBG
        isFakeDst = false;
#endif
        m_kind = oldOpnd.m_kind;
    }
public:
    bool                IsConstOpnd() const;
    bool                IsImmediateOpnd() const;
    bool                IsMemoryOpnd() const;
    bool                IsIntConstOpnd() const;
    IntConstOpnd *      AsIntConstOpnd();
    bool                IsFloatConstOpnd() const;
    FloatConstOpnd *    AsFloatConstOpnd();
    bool                IsSimd128ConstOpnd() const;
    Simd128ConstOpnd *  AsSimd128ConstOpnd();
    bool                IsHelperCallOpnd() const;
    HelperCallOpnd *    AsHelperCallOpnd();
    bool                IsSymOpnd() const;
    SymOpnd *           AsSymOpnd();
    PropertySymOpnd *   AsPropertySymOpnd();
    bool                IsRegOpnd() const;
    const RegOpnd *     AsRegOpnd() const;
    RegOpnd *           AsRegOpnd();
    bool                IsAddrOpnd() const;
    AddrOpnd *          AsAddrOpnd();
    bool                IsIndirOpnd() const;
    IndirOpnd *         AsIndirOpnd();
    bool                IsLabelOpnd() const;
    LabelOpnd *         AsLabelOpnd();
    bool                IsMemRefOpnd() const;
    MemRefOpnd *        AsMemRefOpnd();
    bool                IsRegBVOpnd() const;
    RegBVOpnd *         AsRegBVOpnd();

    OpndKind            GetKind() const;
    Opnd *              Copy(Func *func);
    Opnd *              CloneDef(Func *func);
    Opnd *              CloneUse(Func *func);
    StackSym *          GetStackSym() const;
    Opnd *              UseWithNewType(IRType type, Func * func);

    bool                IsEqual(Opnd *opnd);
    void                Free(Func * func);
    bool                IsInUse() const { return m_inUse; }
    Opnd *              Use(Func * func);
    void                UnUse();
    IRType              GetType() const { return this->m_type; }
    void                SetType(IRType type) { this->m_type = type; }
    bool                IsSigned() const { return IRType_IsSignedInt(this->m_type); }
    bool                IsUnsigned() const { return IRType_IsUnsignedInt(this->m_type); }
    int                 GetSize() const { return TySize[this->m_type]; }
    bool                IsInt64() const { return this->m_type == TyInt64; }
    bool                IsInt32() const { return this->m_type == TyInt32; }
    bool                IsUInt32() const { return this->m_type == TyUint32; }
    bool                IsFloat32() const { return this->m_type == TyFloat32; }
    bool                IsFloat64() const { return this->m_type == TyFloat64; }
    bool                IsFloat() const { return this->IsFloat32() || this->IsFloat64(); }
    bool                IsSimd128() const { return IRType_IsSimd128(this->m_type);  }
    bool                IsSimd128F4() const { return this->m_type == TySimd128F4; }
    bool                IsSimd128I4() const { return this->m_type == TySimd128I4; }
    bool                IsSimd128D2() const { return this->m_type == TySimd128D2; }
    bool                IsVar() const { return this->m_type == TyVar; }
    bool                IsTaggedInt() const;
    bool                IsTaggedValue() const;
    bool                IsNotNumber() const;
    bool                IsNotInt() const;
    bool                IsNotTaggedValue() const;
    bool                IsWriteBarrierTriggerableValue();
    void                SetIsDead(const bool isDead = true)   { this->m_isDead = isDead; }
    bool                GetIsDead()   { return this->m_isDead; }
    intptr_t            GetImmediateValue();
    BailoutConstantValue GetConstValue();
    bool                GetIsJITOptimizedReg() const { return m_isJITOptimizedReg; }
    void                SetIsJITOptimizedReg(bool value) { Assert(!value || !this->IsIndirOpnd()); m_isJITOptimizedReg = value; }

    ValueType           GetValueType() const { return m_valueType; }
    void                SetValueType(const ValueType valueType);
    ValueType           FindProfiledValueType();
#if DBG_DUMP || defined(ENABLE_IR_VIEWER)
    virtual void        DummyFunction() {} // Note needed for the VS debugger to disambiguate the different classes.
    void                DumpValueType();
    static void         DumpValueType(const ValueType valueType);
#endif

    bool                IsValueTypeFixed() const { return m_isValueTypeFixed; }
    void                SetValueTypeFixed() { m_isValueTypeFixed = true; }
    IR::RegOpnd *       FindRegUse(IR::RegOpnd *regOpnd);
    bool                IsArgumentsObject();

    static IntConstOpnd *CreateUint32Opnd(const uint i, Func *const func);
    static IntConstOpnd *CreateProfileIdOpnd(const Js::ProfileId profileId, Func *const func);
    static IntConstOpnd *CreateInlineCacheIndexOpnd(const Js::InlineCacheIndex inlineCacheIndex, Func *const func);
    static RegOpnd *CreateFramePointerOpnd(Func *const func);
public:
#if DBG_DUMP || defined(ENABLE_IR_VIEWER)
    static void         DumpAddress(void *address, bool printToConsole, bool skipMaskedAddress);
    static void         DumpFunctionInfo(_Outptr_result_buffer_(*count) wchar_t ** buffer, size_t * count, Js::FunctionInfo * info, bool printToConsole, _In_opt_z_ wchar_t const * type = nullptr);
    void                Dump(IRDumpFlags flags, Func *func);
    void                DumpOpndKindAddr(bool AsmDumpMode, Func *func);
    void                DumpOpndKindMemRef(bool AsmDumpMode, Func *func);
    static void         WriteToBuffer(_Outptr_result_buffer_(*count) wchar_t **buffer, size_t *count, const wchar_t *fmt, ...);
    void                GetAddrDescription(__out_ecount(count) wchar_t *const description, const size_t count, bool AsmDumpMode,
                            bool printToConsole, Func *func);
    static void         GetAddrDescription(__out_ecount(count) wchar_t *const description, const size_t count,
                            void * address, IR::AddrOpndKind addressKind, bool AsmDumpMode, bool printToConsole, Func *func, bool skipMaskedAddress = false);
    void                Dump();
#endif

    bool                CanStoreTemp() const { return canStoreTemp; }
    void                SetCanStoreTemp() { Assert(this->IsSymOpnd() || this->IsIndirOpnd()); canStoreTemp = true; }
protected:
    ValueType           m_valueType;
    IRType              m_type;

    // If true, it was deemed that the value type is definite (not likely) and shouldn't be changed. This is used for NewScArray
    // and the store-element instructions that follow it.
    bool                m_isValueTypeFixed:1;

    bool                m_inUse:1;
    bool                m_isDead:1;
    // This def/use of a byte code sym is not in the original byte code, don't count them in the bailout
    bool                m_isJITOptimizedReg:1;

    // For SymOpnd, this bit applies to the object pointer stack sym
    // For IndirOpnd, this bit applies to the base operand

    // If this opnd is a dst, that means that the object pointer is a stack object,
    // and we can store temp object/number on it
    // If the opnd is a src, that means that the object pointer may be a stack object
    // so the load may be a temp object/number and we need to track its use
    bool                canStoreTemp : 1;

    bool                isDiagHelperCallOpnd : 1;
    bool                isPropertySymOpnd : 1;
public:
#if DBG
    bool                isFakeDst:1;
#endif
    OpndKind            m_kind;

};

///---------------------------------------------------------------------------
///
/// class IntConstOpnd
///
///---------------------------------------------------------------------------

class IntConstOpnd sealed : public Opnd
{
public:
    static IntConstOpnd *   New(IntConstType value, IRType type, Func *func, bool dontEncode = false);
#if DBG_DUMP || defined(ENABLE_IR_VIEWER)
    static IntConstOpnd *   New(IntConstType value, IRType type, const wchar_t * name, Func *func, bool dontEncode = false);
#endif

public:
    //Note: type OpndKindIntConst
    IntConstOpnd *          CopyInternal(Func *func);
    bool                    IsEqualInternal(Opnd *opnd);
    void                    FreeInternal(Func * func) ;
public:
    bool                    m_dontEncode;       // Setting this to true turns off XOR encoding for this constant.  Only set this on
                                                // constants not controllable by the user.

    IntConstType GetValue()
    {
        return m_value;
    }

    void IncrValue(IntConstType by)
    {
        SetValue(m_value + by);
    }

    void DecrValue(IntConstType by)
    {
        SetValue(m_value - by);
    }

    void SetValue(IntConstType value);
    int32 AsInt32();
    uint32 AsUint32();

#if DBG_DUMP || defined(ENABLE_IR_VIEWER)
    IntConstType            decodedValue;  // FIXME (t-doilij) set ENABLE_IR_VIEWER blocks where this is set
    wchar_t const *         name;  // FIXME (t-doilij) set ENABLE_IR_VIEWER blocks where this is set
#endif

private:
    IntConstType            m_value;
};

///---------------------------------------------------------------------------
///
/// class FloatConstOpnd
///
///---------------------------------------------------------------------------

class FloatConstOpnd: public Opnd
{
public:
    static FloatConstOpnd * New(FloatConstType value, IRType type, Func *func);
    static FloatConstOpnd * New(Js::Var floatVar, IRType type, Func *func);

public:
    //Note: type OpndKindFloatConst
    FloatConstOpnd         *CopyInternal(Func *func);
    bool                    IsEqualInternal(Opnd *opnd);
    void                    FreeInternal(Func * func);
    AddrOpnd               *GetAddrOpnd(Func *func, bool dontEncode = false);
public:
    FloatConstType          m_value;
private:
#if !FLOATVAR
    Js::Var                 m_number;
#endif
};

class Simd128ConstOpnd sealed : public Opnd
{

public:
    static Simd128ConstOpnd * New(AsmJsSIMDValue value, IRType type, Func *func);

public:

    Simd128ConstOpnd *      CopyInternal(Func *func);
    bool                    IsEqualInternal(Opnd *opnd);
    void                    FreeInternal(Func * func);

public:
    AsmJsSIMDValue          m_value;
};

///---------------------------------------------------------------------------
///
/// class HelperCallOpnd
///
///---------------------------------------------------------------------------

class HelperCallOpnd: public Opnd
{
public:
    static HelperCallOpnd * New(JnHelperMethod fnHelper, Func *func);

protected:
    void Init(JnHelperMethod fnHelper);

public:
    //Note type : OpndKindHelperCall
    HelperCallOpnd         *CopyInternal(Func *func);
    bool                    IsEqualInternal(Opnd *opnd);
    void                    FreeInternal(Func * func);
    bool                    IsDiagHelperCallOpnd() const
    {
        Assert(this->DbgIsDiagHelperCallOpnd() == isDiagHelperCallOpnd);
        return isDiagHelperCallOpnd;
    }
public:
    JnHelperMethod m_fnHelper;

#if DBG
private:
    virtual bool DbgIsDiagHelperCallOpnd() const { return false; }
#endif
};

///---------------------------------------------------------------------------
///
/// class DiagHelperCallOpnd
/// Used in debug mode (Fast F12) for wrapping original helper method with try-catch wrapper.
///
///---------------------------------------------------------------------------

class DiagHelperCallOpnd: public HelperCallOpnd
{
public:
    static DiagHelperCallOpnd * New(JnHelperMethod fnHelper, Func *func, int argCount);
public:
    DiagHelperCallOpnd     *CopyInternalSub(Func *func);
    bool                    IsEqualInternalSub(Opnd *opnd);
public:
    int                     m_argCount;

#if DBG
private:
    virtual bool DbgIsDiagHelperCallOpnd() const override { return true; }
#endif
};


///---------------------------------------------------------------------------
///
/// class SymOpnd
///
///---------------------------------------------------------------------------

class SymOpnd: public Opnd
{
public:
    static SymOpnd *        New(Sym *sym, IRType type, Func *func);
    static SymOpnd *        New(Sym *sym, uint32 offset, IRType type, Func *func);

public:
    // Note type: OpndKindSym
    SymOpnd *               CopyInternal(Func *func);
    SymOpnd *               CloneDefInternal(Func *func);
    SymOpnd *               CloneUseInternal(Func *func);
    StackSym *              GetStackSymInternal() const;
    bool                    IsEqualInternal(Opnd *opnd);
    void                    FreeInternal(Func * func);
    bool                    IsPropertySymOpnd() const
    {
        Assert(this->DbgIsPropertySymOpnd() == this->isPropertySymOpnd);
        return isPropertySymOpnd;
    }
public:
    Sym *                   m_sym;
    uint32                  m_offset;

private:
#if DBG
    virtual bool            DbgIsPropertySymOpnd() const { return false; }
#endif

private:
    ValueType propertyOwnerValueType;

public:
    ValueType GetPropertyOwnerValueType() const
    {
        return propertyOwnerValueType;
    }

    void SetPropertyOwnerValueType(const ValueType valueType)
    {
        propertyOwnerValueType = valueType;
    }

    RegOpnd *CreatePropertyOwnerOpnd(Func *const func) const;
};

class PropertySymOpnd sealed : public SymOpnd
{
protected:
    PropertySymOpnd() : SymOpnd() {}
    PropertySymOpnd(SymOpnd* symOpnd) : SymOpnd(*symOpnd) {}

public:
    static PropertySymOpnd * New(PropertySym *propertySym, uint inlineCacheIndex, IRType type, Func *func);

public:
    PropertySymOpnd * CopyCommon(Func *func);
    PropertySymOpnd * CopyWithoutFlowSensitiveInfo(Func *func);
    PropertySymOpnd * CopyForTypeCheckOnly(Func *func);
    PropertySymOpnd * CopyInternalSub(Func *func);
    PropertySymOpnd * CloneDefInternalSub(Func *func);
    PropertySymOpnd * CloneUseInternalSub(Func *func);
    void              Init(uint inlineCacheIndex, Func *func);

private:
    static PropertySymOpnd * New(PropertySym *propertySym, IRType type, Func *func);
    void Init(uint inlineCacheIndex, Js::InlineCache * runtimeInlineCache, Js::PolymorphicInlineCache * runtimePolymorphicInlineCache, Js::ObjTypeSpecFldInfo* objTypeSpecFldInfo, byte polyCacheUtil);
#if DBG
    virtual bool      DbgIsPropertySymOpnd() const override { return true; }
#endif
public:
    Js::InlineCacheIndex m_inlineCacheIndex;
    Js::InlineCache* m_runtimeInlineCache;
    Js::PolymorphicInlineCache* m_runtimePolymorphicInlineCache;
private:
    Js::ObjTypeSpecFldInfo* objTypeSpecFldInfo;
public:
    Js::Type* finalType;
    BVSparse<JitArenaAllocator>* guardedPropOps;
    BVSparse<JitArenaAllocator>* writeGuards;
    byte m_polyCacheUtil;

private:
    bool usesAuxSlot : 1;
    Js::PropertyIndex slotIndex;
    uint16 checkedTypeSetIndex;

public:
    union
    {
        struct
        {
            bool isTypeCheckOnly: 1;
            // Note that even usesFixedValue cannot live on ObjTypeSpecFldInfo, because we may share a cache between
            // e.g. Object.prototype and new Object(), and only the latter actually uses the fixed value, even though both have it.
            bool usesFixedValue: 1;

            union
            {
                struct
                {
                    bool isTypeCheckSeqCandidate: 1;
                    bool typeAvailable: 1;
                    bool typeDead: 1;
                    bool typeChecked: 1;
                    bool initialTypeChecked: 1;
                    bool mustDoMonoCheck: 1;
                    bool typeMismatch: 1;
                    bool writeGuardChecked: 1;
                };
                uint8 typeCheckSeqFlags;
            };
        };
        uint16 objTypeSpecFlags;
    };

public:
    StackSym * GetObjectSym() const { return this->m_sym->AsPropertySym()->m_stackSym; };
    bool HasObjectTypeSym() const { return this->m_sym->AsPropertySym()->HasObjectTypeSym(); };
    StackSym * GetObjectTypeSym() const { return this->m_sym->AsPropertySym()->GetObjectTypeSym(); };
    PropertySym* GetPropertySym() const { return this->m_sym->AsPropertySym(); }

    void TryDisableRuntimePolymorphicCache()
    {
        if (this->m_runtimePolymorphicInlineCache && (this->m_polyCacheUtil < PolymorphicInlineCacheUtilizationThreshold))
        {
            this->m_runtimePolymorphicInlineCache = nullptr;
        }
    }

    bool HasObjTypeSpecFldInfo() const
    {
        return this->objTypeSpecFldInfo != nullptr;
    }

    void SetObjTypeSpecFldInfo(Js::ObjTypeSpecFldInfo *const objTypeSpecFldInfo)
    {
        this->objTypeSpecFldInfo = objTypeSpecFldInfo;

        // The following information may change in a flow-based manner, and an ObjTypeSpecFldInfo is shared among several
        // PropertySymOpnds, so copy the information to the opnd
        if(!objTypeSpecFldInfo)
        {
            usesAuxSlot = false;
            slotIndex = 0;
            return;
        }
        usesAuxSlot = objTypeSpecFldInfo->UsesAuxSlot();
        slotIndex = objTypeSpecFldInfo->GetSlotIndex();
    }

    void TryResetObjTypeSpecFldInfo()
    {
        if (this->ShouldResetObjTypeSpecFldInfo())
        {
            SetObjTypeSpecFldInfo(nullptr);
        }
    }

    bool ShouldResetObjTypeSpecFldInfo()
    {
        // If an objTypeSpecFldInfo was created just for the purpose of polymorphic inlining but didn't get used for the same (for some reason or the other), and the polymorphic cache it was created from, wasn't equivalent,
        // we should null out this info on the propertySymOpnd so that assumptions downstream around equivalent object type spec still hold.
        if (HasObjTypeSpecFldInfo() && IsPoly() && (DoesntHaveEquivalence() || !IsLoadedFromProto()))
        {
            return true;
        }
        return false;
    }

    Js::ObjTypeSpecFldInfo* GetObjTypeSpecInfo() const
    {
        return this->objTypeSpecFldInfo;
    }

    uint GetObjTypeSpecFldId() const
    {
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetObjTypeSpecFldId();
    }

    bool IsMono() const
    {
        return HasObjTypeSpecFldInfo() && this->objTypeSpecFldInfo->IsMono();
    }

    bool IsPoly() const
    {
        return HasObjTypeSpecFldInfo() && this->objTypeSpecFldInfo->IsPoly();
    }

    bool HasEquivalentTypeSet() const
    {
        return HasObjTypeSpecFldInfo() && this->objTypeSpecFldInfo->HasEquivalentTypeSet();
    }

    bool DoesntHaveEquivalence() const
    {
        return HasObjTypeSpecFldInfo() && this->objTypeSpecFldInfo->DoesntHaveEquivalence();
    }

    bool UsesAuxSlot() const
    {
        return usesAuxSlot && HasObjTypeSpecFldInfo();
    }

    void SetUsesAuxSlot(bool value)
    {
        Assert(HasObjTypeSpecFldInfo());
        usesAuxSlot = value;
    }

    bool IsLoadedFromProto() const
    {
        return HasObjTypeSpecFldInfo() && this->objTypeSpecFldInfo->IsLoadedFromProto();
    }

    bool UsesAccessor() const
    {
        return HasObjTypeSpecFldInfo() && this->objTypeSpecFldInfo->UsesAccessor();
    }

    bool HasFixedValue() const
    {
        return HasObjTypeSpecFldInfo() && this->objTypeSpecFldInfo->HasFixedValue();
    }

    bool UsesFixedValue() const
    {
        return this->usesFixedValue;
    }

    void SetUsesFixedValue(bool value)
    {
        this->usesFixedValue = value;
    }

    bool MustDoMonoCheck() const
    {
        // Question: does this property access need to do a monomorphic check because of some other access
        // that this check protects?
        return this->mustDoMonoCheck;
    }

    void SetMustDoMonoCheck(bool value)
    {
        this->mustDoMonoCheck = value;
    }

    bool NeedsMonoCheck() const
    {
        Assert(HasObjTypeSpecFldInfo());
        return this->IsBeingAdded() || (this->HasFixedValue() && !this->IsLoadedFromProto());
    }

    bool IsBeingStored() const
    {
        return HasObjTypeSpecFldInfo() && this->objTypeSpecFldInfo->IsBeingStored();
    }

    void SetIsBeingStored(bool value)
    {
        Assert(HasObjTypeSpecFldInfo());
        this->objTypeSpecFldInfo->SetIsBeingStored(value);
    }

    bool IsBeingAdded() const
    {
        return HasObjTypeSpecFldInfo() && this->objTypeSpecFldInfo->IsBeingAdded();
    }

    void SetIsBeingAdded(bool value)
    {
        Assert(HasObjTypeSpecFldInfo());
        this->objTypeSpecFldInfo->SetIsBeingAdded(value);
    }

    bool IsRootObjectNonConfigurableField() const
    {
        return HasObjTypeSpecFldInfo() && this->objTypeSpecFldInfo->IsRootObjectNonConfigurableField();
    }

    bool IsRootObjectNonConfigurableFieldLoad() const
    {
        return HasObjTypeSpecFldInfo() && this->objTypeSpecFldInfo->IsRootObjectNonConfigurableFieldLoad();
    }

    uint16 GetSlotIndex() const
    {
        Assert(HasObjTypeSpecFldInfo());
        return slotIndex;
    }

    void SetSlotIndex(uint16 index)
    {
        Assert(HasObjTypeSpecFldInfo());
        slotIndex = index;
    }

    uint16 GetCheckedTypeSetIndex() const
    {
        Assert(HasEquivalentTypeSet());
        return checkedTypeSetIndex;
    }

    void SetCheckedTypeSetIndex(uint16 index)
    {
        Assert(HasEquivalentTypeSet());
        checkedTypeSetIndex = index;
    }

    Js::PropertyId GetPropertyId() const
    {
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetPropertyId();
    }

    Js::DynamicObject* GetProtoObject() const
    {
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetProtoObject();
    }

    Js::JavascriptFunction* GetFieldValueAsFixedFunction() const
    {
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetFieldValueAsFixedFunctionIfAvailable();
    }

    Js::JavascriptFunction* GetFieldValueAsFixedFunction(uint i) const
    {
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetFieldValueAsFixedFunctionIfAvailable(i);
    }

    Js::Var GetFieldValueAsFixedData() const
    {
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetFieldValueAsFixedDataIfAvailable();
    }

    Js::Var GetFieldValue(uint i)
    {
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetFieldValue(i);
    }

    Js::FixedFieldInfo* GetFixedFieldInfoArray()
    {
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetFixedFieldInfoArray();
    }

    uint16 GetFixedFieldCount()
    {
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetFixedFieldCount();
    }

    Js::JitTimeConstructorCache* GetCtorCache() const
    {
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetCtorCache();
    }

    Js::PropertyGuard* GetPropertyGuard() const
    {
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetPropertyGuard();
    }

    bool IsTypeCheckSeqCandidate() const
    {
        Assert(IsObjTypeSpecCandidate() || !this->isTypeCheckSeqCandidate);
        return this->isTypeCheckSeqCandidate;
    }

    void SetTypeCheckSeqCandidate(bool value)
    {
        Assert(IsObjTypeSpecCandidate() || !value);
        this->isTypeCheckSeqCandidate = value;
    }

    void SetTypeCheckSeqCandidateIfObjTypeSpecCandidate()
    {
        if (IsObjTypeSpecCandidate())
        {
            this->isTypeCheckSeqCandidate = true;
        }
    }

    bool IsTypeCheckOnly() const
    {
        return this->isTypeCheckOnly;
    }

    void SetTypeCheckOnly(bool value)
    {
        this->isTypeCheckOnly = value;
    }

    bool IsTypeAvailable() const
    {
        return this->typeAvailable;
    }

    void SetTypeAvailable(bool value)
    {
        Assert(IsTypeCheckSeqCandidate());
        this->typeAvailable = value;
    }

    bool IsTypeDead() const
    {
        return this->typeDead;
    }

    void SetTypeDead(bool value)
    {
        Assert(IsTypeCheckSeqCandidate());
        this->typeDead = value;
    }

    void SetTypeDeadIfTypeCheckSeqCandidate(bool value)
    {
        if (IsTypeCheckSeqCandidate())
        {
            this->typeDead = value;
        }
    }

    bool IsTypeChecked() const
    {
        return this->typeChecked;
    }

    void SetTypeChecked(bool value)
    {
        Assert(IsTypeCheckSeqCandidate());
        this->typeChecked = value;
    }

    bool IsInitialTypeChecked() const
    {
        return this->initialTypeChecked;
    }

    void SetInitialTypeChecked(bool value)
    {
        Assert(IsTypeCheckSeqCandidate());
        this->initialTypeChecked = value;
    }

    bool HasTypeMismatch() const
    {
        return this->typeMismatch;
    }

    void SetTypeMismatch(bool value)
    {
        Assert(IsTypeCheckSeqCandidate());
        this->typeMismatch = value;
    }

    bool IsWriteGuardChecked() const
    {
        return this->writeGuardChecked;
    }

    void SetWriteGuardChecked(bool value)
    {
        Assert(IsTypeCheckSeqCandidate());
        this->writeGuardChecked = value;
    }

    uint16 GetObjTypeSpecFlags() const
    {
        return this->objTypeSpecFlags;
    }

    void ClearObjTypeSpecFlags()
    {
        this->objTypeSpecFlags = 0;
    }

    uint16 GetTypeCheckSeqFlags() const
    {
        return this->typeCheckSeqFlags;
    }

    void ClearTypeCheckSeqFlags()
    {
        this->typeCheckSeqFlags = 0;
    }

    bool MayNeedTypeCheckProtection() const
    {
        return IsObjTypeSpecCandidate() && (IsTypeCheckSeqCandidate() || UsesFixedValue());
    }

    bool MayNeedWriteGuardProtection() const
    {
        return IsLoadedFromProto() || UsesFixedValue();
    }

    bool IsTypeCheckProtected() const
    {
        return IsTypeCheckSeqCandidate() && IsTypeChecked();
    }

    bool NeedsPrimaryTypeCheck() const
    {
        // Only indicate that we need a primary type check, i.e. the type isn't yet available but will be needed downstream.
        // Type checks and bailouts may still be needed in other places (e.g. loads from proto, fixed field checks, or
        // property adds), if a primary type check cannot protect them.
        Assert(MayNeedTypeCheckProtection());
        Assert(TypeCheckSeqBitsSetOnlyIfCandidate());
        return IsTypeCheckSeqCandidate() && !IsTypeDead() && !IsTypeChecked() && !HasTypeMismatch();
    }

    bool NeedsLocalTypeCheck() const
    {
        Assert(MayNeedTypeCheckProtection());
        Assert(TypeCheckSeqBitsSetOnlyIfCandidate());
        // Indicate whether this operation needs a type check for its own sake, since the type is dead and no downstream
        // operations require the type to be checked.
        return !PHASE_OFF1(Js::ObjTypeSpecIsolatedFldOpsPhase) &&
            IsTypeCheckSeqCandidate() && IsTypeDead() && !IsTypeCheckOnly() && !IsTypeChecked() && !HasTypeMismatch();
    }

    bool NeedsWriteGuardTypeCheck() const
    {
        Assert(MayNeedTypeCheckProtection());
        Assert(TypeCheckSeqBitsSetOnlyIfCandidate());
        // Type has been checked but property might have been written to since then.
        return !IsTypeCheckOnly() && !NeedsPrimaryTypeCheck() && IsTypeChecked() && !IsWriteGuardChecked();
    }

    bool NeedsLoadFromProtoTypeCheck() const
    {
        Assert(MayNeedTypeCheckProtection());
        Assert(TypeCheckSeqBitsSetOnlyIfCandidate());
        // Proto cache, where type has been checked but property might have been written to since then.
        return !IsTypeCheckOnly() && !NeedsPrimaryTypeCheck() && IsLoadedFromProto() && NeedsWriteGuardTypeCheck();
    }

    bool NeedsAddPropertyTypeCheck() const
    {
        Assert(MayNeedTypeCheckProtection());
        Assert(TypeCheckSeqBitsSetOnlyIfCandidate());
        // A property cannot become read-only without an explicit or implicit call (at least Object.defineProperty is needed), so if this
        // operation is protected by a primary type check upstream, there is no need for an additional local type check.
        return false;
    }

    bool NeedsCheckFixedFieldTypeCheck() const
    {
        Assert(MayNeedTypeCheckProtection());
        Assert(TypeCheckSeqBitsSetOnlyIfCandidate());
        return !IsTypeCheckOnly() && !NeedsPrimaryTypeCheck() && UsesFixedValue() && (!IsTypeChecked() || NeedsWriteGuardTypeCheck());
    }

    bool NeedsTypeCheck() const
    {
        return NeedsPrimaryTypeCheck() || NeedsLocalTypeCheck() ||
            NeedsLoadFromProtoTypeCheck() || NeedsAddPropertyTypeCheck() || NeedsCheckFixedFieldTypeCheck();
    }

    bool NeedsTypeCheckAndBailOut() const
    {
        return NeedsPrimaryTypeCheck() || (PHASE_ON1(Js::ObjTypeSpecIsolatedFldOpsWithBailOutPhase) && NeedsLocalTypeCheck()) || NeedsCheckFixedFieldTypeCheck();
    }

    // Is the instruction involving this operand optimized with a direct slot load or store? In other words, is it guarded
    // by a type check, either as part of the type check sequence, or explicitly on this instruction.
    bool IsObjTypeSpecOptimized() const
    {
        return MayNeedTypeCheckProtection() && (NeedsTypeCheckAndBailOut() || IsTypeCheckProtected());
    }

    // May the instruction involving this operand result in an implicit call?  Note, that because in dead store pass we
    // may choose to remove a type check and fall back on a check against a live cache, instructions that have a primary
    // type check may still end up with implicit call bailout.  However, if we are type check protected we will never
    // fall back on live cache.  Similarly, for fixed method checks.
    bool MayHaveImplicitCall() const
    {
        return !IsRootObjectNonConfigurableFieldLoad() && !UsesFixedValue() && (!IsTypeCheckSeqCandidate() || !IsTypeCheckProtected());
    }

    // Is the instruction involving this operand part of a type check sequence? This is different from IsObjTypeSpecOptimized
    // in that an instruction such as CheckFixedFld may require a type check even if it is not part of a type check
    // sequence. In this case IsObjTypeSpecOptimized() == true, but IsTypeCheckSeqParticipant() == false.
    bool IsTypeCheckSeqParticipant() const
    {
        Assert(IsTypeCheckSeqCandidate());
        return NeedsPrimaryTypeCheck() || IsTypeCheckProtected();
    }

    bool HasFinalType() const
    {
        return this->finalType != nullptr;
    }

    Js::Type * GetFinalType() const
    {
        return this->finalType;
    }

    void SetFinalType(Js::Type* type)
    {
        Assert(type != nullptr);
        this->finalType = type;
    }

    void ClearFinalType()
    {
        this->finalType = nullptr;
    }

    BVSparse<JitArenaAllocator>* GetGuardedPropOps()
    {
        return this->guardedPropOps;
    }

    void EnsureGuardedPropOps(JitArenaAllocator* allocator)
    {
        if (this->guardedPropOps == nullptr)
        {
            this->guardedPropOps = JitAnew(allocator, BVSparse<JitArenaAllocator>, allocator);
        }
    }

    void SetGuardedPropOp(uint propOpId)
    {
        Assert(this->guardedPropOps != nullptr);
        this->guardedPropOps->Set(propOpId);
    }

    void AddGuardedPropOps(const BVSparse<JitArenaAllocator>* propOps)
    {
        Assert(this->guardedPropOps != nullptr);
        this->guardedPropOps->Or(propOps);
    }

    BVSparse<JitArenaAllocator>* GetWriteGuards()
    {
        return this->writeGuards;
    }

    void SetWriteGuards(BVSparse<JitArenaAllocator>* value)
    {
        Assert(this->writeGuards == nullptr);
        this->writeGuards = value;
    }

    void ClearWriteGuards()
    {
        this->writeGuards = nullptr;
    }

#if DBG
    bool TypeCheckSeqBitsSetOnlyIfCandidate() const
    {
        return IsTypeCheckSeqCandidate() || (!IsTypeAvailable() && !IsTypeChecked() && !IsWriteGuardChecked() && !IsTypeDead());
    }
#endif

    bool IsObjTypeSpecCandidate() const
    {
        return HasObjTypeSpecFldInfo() && this->objTypeSpecFldInfo->IsObjTypeSpecCandidate();
    }

    bool IsMonoObjTypeSpecCandidate() const
    {
        return HasObjTypeSpecFldInfo() && this->objTypeSpecFldInfo->IsMonoObjTypeSpecCandidate();
    }

    bool IsPolyObjTypeSpecCandidate() const
    {
        return HasObjTypeSpecFldInfo() && this->objTypeSpecFldInfo->IsPolyObjTypeSpecCandidate();
    }

    Js::TypeId GetTypeId() const
    {
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetTypeId();
    }

    Js::TypeId GetTypeId(uint i) const
    {
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetTypeId(i);
    }

    Js::Type * GetType() const
    {
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetType();
    }

    Js::Type * GetType(uint i) const
    {
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetType(i);
    }

    bool HasInitialType() const
    {
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->HasInitialType();
    }

    Js::Type * GetInitialType() const
    {
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetInitialType();
    }

    Js::EquivalentTypeSet * GetEquivalentTypeSet() const
    {
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetEquivalentTypeSet();
    }

    Js::Type * GetFirstEquivalentType() const
    {
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetFirstEquivalentType();
    }

    bool IsObjectHeaderInlined() const;
    void UpdateSlotForFinalType();
    bool ChangesObjectLayout() const;

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    const wchar_t* GetCacheLayoutString() const
    {
        return HasObjTypeSpecFldInfo() ? this->objTypeSpecFldInfo->GetCacheLayoutString() : L"empty";
    }
#endif

};

///---------------------------------------------------------------------------
///
/// class RegOpnd
///
///---------------------------------------------------------------------------

class RegOpnd : public Opnd
{
protected:
    RegOpnd(StackSym *sym, RegNum reg, IRType type);
    RegOpnd(const RegOpnd &other, StackSym * sym);
private:
    void                    Initialize(StackSym *sym, RegNum reg, IRType type);

public:
    static RegOpnd *        New(IRType type, Func *func);
    static RegOpnd *        New(StackSym *sym, IRType type, Func *func);
    static RegOpnd *        New(StackSym *sym, RegNum reg, IRType type, Func *func);

public:
    bool                    IsArrayRegOpnd() const
    {
        Assert(m_isArrayRegOpnd == DbgIsArrayRegOpnd());
        Assert(!m_isArrayRegOpnd || m_valueType.IsAnyOptimizedArray());
        return m_isArrayRegOpnd;
    }

    ArrayRegOpnd *          AsArrayRegOpnd();

    RegNum                  GetReg() const;
    void                    SetReg(RegNum reg);
    //Note type: OpndKindReg
    RegOpnd *               CopyInternal(Func *func);
    RegOpnd *               CloneDefInternal(Func *func);
    RegOpnd *               CloneUseInternal(Func *func);
    StackSym *              GetStackSymInternal() const;
    static StackSym *       TryGetStackSym(Opnd *const opnd);
    bool                    IsEqualInternal(Opnd *opnd);
    void                    FreeInternal(Func * func);
    bool                    IsSameReg(Opnd *opnd);
    bool                    IsSameRegUntyped(Opnd *opnd);

#if DBG
    void FreezeSymValue() { m_symValueFrozen = true; }
    bool IsSymValueFrozen() const { return m_symValueFrozen; }

    virtual bool DbgIsArrayRegOpnd() const { return false; }
#endif

private:
    RegOpnd *               CopyInternal(StackSym * sym, Func * func);

public:
    StackSym *              m_sym;
    bool                    m_isTempLastUse:1;
    bool                    m_isCallArg:1;
    bool                    m_dontDeadStore: 1;
    bool                    m_fgPeepTmp: 1;
    bool                    m_wasNegativeZeroPreventedByBailout : 1;
    bool                    m_isArrayRegOpnd : 1;
#if DBG
private:
    bool                    m_symValueFrozen : 1; // if true, prevents this operand from being used as the destination operand in an instruction
#endif

private:
    RegNum                  m_reg;

    PREVENT_COPY(RegOpnd);
};

///---------------------------------------------------------------------------
///
/// class ArrayRegOpnd
///
///---------------------------------------------------------------------------

class ArrayRegOpnd sealed : public RegOpnd
{
private:
    StackSym *headSegmentSym;
    StackSym *headSegmentLengthSym;
    StackSym *lengthSym;
    const bool eliminatedLowerBoundCheck, eliminatedUpperBoundCheck;

protected:
    ArrayRegOpnd(StackSym *const arraySym, const ValueType valueType, StackSym *const headSegmentSym, StackSym *const headSegmentLengthSym, StackSym *const lengthSym, const bool eliminatedLowerBoundCheck, const bool eliminatedUpperBoundCheck);
    ArrayRegOpnd(const RegOpnd &other, StackSym *const arraySym, const ValueType valueType, StackSym *const headSegmentSym, StackSym *const headSegmentLengthSym, StackSym *const lengthSym, const bool eliminatedLowerBoundCheck, const bool eliminatedUpperBoundCheck);

public:
    static ArrayRegOpnd *New(StackSym *const arraySym, const ValueType valueType, StackSym *const headSegmentSym, StackSym *const headSegmentLengthSym, StackSym *const lengthSym, const bool eliminatedLowerBoundCheck, const bool eliminatedUpperBoundCheck, Func *const func);
    static ArrayRegOpnd *New(const RegOpnd *const other, const ValueType valueType, StackSym *const headSegmentSym, StackSym *const headSegmentLengthSym, StackSym *const lengthSym, const bool eliminatedLowerBoundCheck, const bool eliminatedUpperBoundCheck, Func *const func);

public:
#if DBG
    virtual bool DbgIsArrayRegOpnd() const { return true; }
#endif
    StackSym *HeadSegmentSym() const
    {
        return headSegmentSym;
    }

    void RemoveHeadSegmentSym()
    {
        headSegmentSym = nullptr;
    }

    StackSym *HeadSegmentLengthSym() const
    {
        return headSegmentLengthSym;
    }

    void RemoveHeadSegmentLengthSym()
    {
        headSegmentLengthSym = nullptr;
    }

    StackSym *LengthSym() const
    {
        // For typed arrays, the head segment length is the same as the array length
        Assert(!(m_valueType.IsLikelyTypedArray() && !m_valueType.IsOptimizedTypedArray()));
        return m_valueType.IsLikelyTypedArray() ? HeadSegmentLengthSym() : lengthSym;
    }

    void RemoveLengthSym()
    {
        Assert(m_valueType.IsArray());
        lengthSym = nullptr;
    }

    bool EliminatedLowerBoundCheck() const
    {
        return eliminatedLowerBoundCheck;
    }

    bool EliminatedUpperBoundCheck() const
    {
        return eliminatedUpperBoundCheck;
    }

public:
    RegOpnd *CopyAsRegOpnd(Func *func);
    ArrayRegOpnd * CopyInternalSub(Func *func);
    ArrayRegOpnd *CloneDefInternalSub(Func *func);
    ArrayRegOpnd *CloneUseInternalSub(Func *func);
private:
    ArrayRegOpnd *Clone(StackSym *const arraySym, StackSym *const headSegmentSym, StackSym *const headSegmentLengthSym, StackSym *const lengthSym, Func *const func) const;

public:
    void FreeInternalSub(Func *func);

    // IsEqual is not overridden because this opnd still primarily represents the array sym. Equality comparisons using IsEqual
    // are used to determine whether opnds should be swapped, etc. and the extra information in this class should not affect
    // that behavior.
    // virtual bool IsEqual(Opnd *opnd) override;

    PREVENT_COPY(ArrayRegOpnd);
};

///---------------------------------------------------------------------------
///
/// class AddrOpnd
///
///---------------------------------------------------------------------------

class AddrOpnd sealed : public Opnd
{
public:
    static AddrOpnd *       New(Js::Var address, AddrOpndKind addrOpndKind, Func *func, bool dontEncode = false);
    static AddrOpnd *       NewFromNumber(double value, Func *func, bool dontEncode = false);
    static AddrOpnd *       NewFromNumber(int32 value, Func *func, bool dontEncode = false);
    static AddrOpnd *       NewFromNumber(int64 value, Func *func, bool dontEncode = false);
    static AddrOpnd *       NewNull(Func * func);
public:
    //Note type: OpndKindAddr
    AddrOpnd *              CopyInternal(Func *func);
    bool                    IsEqualInternal(Opnd *opnd);
    void                    FreeInternal(Func * func);

    bool                    IsDynamic() const { return addrOpndKind > AddrOpndKindConstantVar; }
    bool                    IsVar() const { return addrOpndKind == AddrOpndKindDynamicVar || addrOpndKind == AddrOpndKindConstantVar; }
    void                    SetEncodedValue(Js::Var address, AddrOpndKind addrOpndKind);
    AddrOpndKind            GetAddrOpndKind() const { return addrOpndKind; }
    void                    SetAddress(Js::Var address, AddrOpndKind addrOpndKind);
public:
    Js::Var                 m_address;
    bool                    m_dontEncode: 1;
    bool                    m_isFunction: 1;
private:
    AddrOpndKind            addrOpndKind;
public:
#if DBG_DUMP || defined(ENABLE_IR_VIEWER)
    Js::Var                 decodedValue;  // FIXME (t-doilij) set ENABLE_IR_VIEWER blocks where this is set
#endif
#if DBG_DUMP || defined(ENABLE_IR_VIEWER)
    bool                    wasVar;
#endif
};

///---------------------------------------------------------------------------
///
/// class IndirOpnd
///
///---------------------------------------------------------------------------

class IndirOpnd: public Opnd
{
public:
    static IndirOpnd *      New(RegOpnd * baseOpnd, RegOpnd * indexOpnd, IRType type, Func *func);
    static IndirOpnd *      New(RegOpnd * baseOpnd, RegOpnd * indexOpnd, byte scale, IRType type, Func *func);
    static IndirOpnd *      New(RegOpnd * baseOpnd, int32 offset, IRType type, Func *func, bool dontEncode = false);
#if DBG_DUMP || defined(ENABLE_IR_VIEWER)
    static IndirOpnd *      New(RegOpnd * baseOpnd, int32 offset, IRType type, const wchar_t *desc, Func *func, bool dontEncode = false);
#endif

public:
    IndirOpnd() : Opnd(), m_baseOpnd(nullptr), m_indexOpnd(nullptr), m_offset(0), m_scale(0), m_func(nullptr), m_dontEncode(false)
#if DBG_DUMP || defined(ENABLE_IR_VIEWER)
        , m_desc(nullptr)
#endif
#if DBG_DUMP
        , m_addrKind((IR::AddrOpndKind)-1)
#endif
    {
    }
    ~IndirOpnd();

    // Note type: OpndKindIndir
    IndirOpnd *             CopyInternal(Func *func);
    IndirOpnd *             CloneDefInternal(Func *func);
    IndirOpnd *             CloneUseInternal(Func *func);
    bool                    IsEqualInternal(Opnd *opnd);
    void                    FreeInternal(Func * func);

    RegOpnd *               GetBaseOpnd() const;
    void                    SetBaseOpnd(RegOpnd *baseOpnd);
    RegOpnd *               UnlinkBaseOpnd();
    void                    ReplaceBaseOpnd(RegOpnd *newBase);
    RegOpnd *               GetIndexOpnd();
    void                    SetIndexOpnd(RegOpnd *indexOpnd);
    RegOpnd *               GetIndexOpnd() const;
    RegOpnd *               UnlinkIndexOpnd();
    void                    ReplaceIndexOpnd(RegOpnd *newIndex);
    int32                   GetOffset() const;
    void                    SetOffset(int32 offset, bool dontEncode = false);
    byte                    GetScale() const;
    void                    SetScale(byte scale);
    bool                    TryGetIntConstIndexValue(bool trySym, IntConstType *pValue, bool *pIsNotInt);
#if DBG_DUMP || defined(ENABLE_IR_VIEWER)
    const wchar_t *         GetDescription();
    IR::AddrOpndKind        GetAddrKind() const;
    bool                    HasAddrKind() const;
    void *                  GetOriginalAddress() const;
#endif
    bool                    m_dontEncode;

#if DBG_DUMP
    void                    SetAddrKind(IR::AddrOpndKind kind, void * originalAddress);
#endif
private:
    RegOpnd *               m_baseOpnd;
    RegOpnd *               m_indexOpnd;
    int32                   m_offset;
    byte                    m_scale;
    Func *                  m_func;  // We need the allocator to copy the base and index...

#if DBG_DUMP || defined(ENABLE_IR_VIEWER)
    const wchar_t *         m_desc;
#endif
#if DBG_DUMP
    IR::AddrOpndKind        m_addrKind;  // if m_addrKind != -1, than this used to be MemRefOpnd which has the address hoisted;
    void *                  m_originalAddress;
#endif

};

///---------------------------------------------------------------------------
///
/// class MemRefOpnd - represents a reference to a fixed memory location
///
///---------------------------------------------------------------------------

class MemRefOpnd : public Opnd
{
public:
    static MemRefOpnd *     New(void * pMemLoc, IRType, Func * func, AddrOpndKind addrOpndKind = AddrOpndKindDynamicMisc);

public:
    // Note type: OpndKindMemRef
    MemRefOpnd *            CopyInternal(Func * func);
    bool                    IsEqualInternal(Opnd *opnd);
    void                    FreeInternal(Func * func);

    void *                  GetMemLoc() const;
    void                    SetMemLoc(void * pMemLoc);

    IR::AddrOpndKind        GetAddrKind() const;

private:
    void *                  m_memLoc;
#if DBG_DUMP
    AddrOpndKind            m_addrKind;
#endif
};

//
// class LabelOpnd - represents a reference to a local code address
//

class LabelOpnd : public Opnd
{
public:
    static LabelOpnd *      New(LabelInstr * labelInstr, Func * func);

public:
    //Note type: OpndKindLabel
    LabelOpnd *             CopyInternal(Func * func);
    bool                    IsEqualInternal(Opnd * opnd);
    void                    FreeInternal(Func * func);

    LabelInstr *            GetLabel() const;
    void                    SetLabel(LabelInstr * labelInstr);

private:
    LabelInstr *            m_label;
};

///---------------------------------------------------------------------------
///
/// class Bit Field vector
///
///---------------------------------------------------------------------------

class RegBVOpnd: public Opnd
{
public:
    static RegBVOpnd *      New(BVUnit32 value, IRType type, Func *func);

public:
    //Note: type: OpndKindRegBV
    RegBVOpnd *             CopyInternal(Func *func);
    bool                    IsEqualInternal(Opnd *opnd);
    void                    FreeInternal(Func * func);
    BVUnit32                GetValue() const;
public:
    BVUnit32                m_value;
};

class AutoReuseOpnd
{
private:
    Opnd *opnd;
    Func *func;
    bool autoDelete;
    bool wasInUse;

public:
    AutoReuseOpnd() : opnd(nullptr), wasInUse(true)
    {
    }

    AutoReuseOpnd(Opnd *const opnd, Func *const func, const bool autoDelete = true) : opnd(nullptr)
    {
        Initialize(opnd, func, autoDelete);
    }

    void Initialize(Opnd *const opnd, Func *const func, const bool autoDelete = true)
    {
        Assert(!this->opnd);
        Assert(func);

        if(!opnd)
        {
            // Simulate the default constructor
            wasInUse = true;
            return;
        }

        this->opnd = opnd;
        wasInUse = opnd->IsInUse();
        if(wasInUse)
        {
            return;
        }
        this->func = func;
        this->autoDelete = autoDelete;

        // Create a fake use of the opnd to enable opnd reuse during lowering. One issue is that when an unused opnd is first
        // used in an instruction and the instruction is legalized, the opnd may be replaced by legalization and the original
        // opnd would be freed. By creating a fake use, it forces the opnd to be copied when used by the instruction, so the
        // original opnd can continue to be reused for other instructions. Typically, any opnds used during lowering in more
        // than one instruction can use this class to enable opnd reuse.
        opnd->Use(func);
    }

    ~AutoReuseOpnd()
    {
        if(wasInUse)
        {
            return;
        }
        if(autoDelete)
        {
            opnd->Free(func);
        }
        else
        {
            opnd->UnUse();
        }
    }

    PREVENT_COPY(AutoReuseOpnd)
};

} // namespace IR
