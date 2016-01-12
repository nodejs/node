//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once
namespace Js
{
    class IDiagObjectAddress;
    class IDiagObjectModelDisplay;
    class RecyclableMethodsGroupWalker;
    class RecyclableObjectWalker;
    class RecyclableArrayWalker;

    // Concrete type for manipulating JS Vars
    struct ResolvedObject
    {
        ResolvedObject() : propId(Js::Constants::NoProperty), scriptContext(nullptr), address(nullptr),
            objectDisplay(nullptr), obj(nullptr), originalObj(nullptr), isConst(false), name(nullptr)
        {}

        PropertyId              propId;
        ScriptContext           *scriptContext;
        IDiagObjectAddress      *address;
        IDiagObjectModelDisplay *objectDisplay;
        Var                     obj;
        Var                     originalObj;
        LPCWSTR                 name;
        TypeId                  typeId;
        bool                    isConst;

        WeakArenaReference<IDiagObjectModelDisplay>* GetObjectDisplay();

        IDiagObjectModelDisplay * CreateDisplay();
        bool IsInDeadZone() const;
    };

    // interfaces for manipulating DataTypes

    // Allow setting the value across different parent data types
    class IDiagObjectAddress
    {
    public:
        virtual BOOL Set(Var updateObject) = 0;
        virtual BOOL IsWritable() { return !IsInDeadZone(); }
        virtual Var GetValue(BOOL fUpdated) { return nullptr; }
        virtual BOOL IsInDeadZone() const { return FALSE; };
    };

    class IDiagObjectModelWalkerBase
    {
    public:
        // Get the child at i'th position.
        virtual BOOL Get(int i, ResolvedObject* pResolvedObject) = 0;

        // Returns number of children for the current diag object.
        virtual ulong GetChildrenCount() = 0;

        virtual BOOL GetGroupObject(ResolvedObject* pResolvedObject) = 0;

        virtual IDiagObjectAddress *FindPropertyAddress(PropertyId propId, bool& isConst) { return nullptr;}
    };

    enum DiagObjectModelDisplayType
    {
        DiagObjectModelDisplayType_LocalsDisplay,
        DiagObjectModelDisplayType_RecyclableObjectDisplay,
        DiagObjectModelDisplayType_RecyclableCollectionObjectDisplay,
        DiagObjectModelDisplayType_RecyclableKeyValueDisplay,
    };

    // Allow getting information across different data types
    class IDiagObjectModelDisplay
    {
    public:
        virtual LPCWSTR Name() = 0;
        virtual LPCWSTR Type() = 0;
        virtual LPCWSTR Value(int radix) = 0;
        virtual BOOL HasChildren() = 0;
        virtual BOOL Set(Var updateObject) = 0;
        virtual DBGPROP_ATTRIB_FLAGS GetTypeAttribute() = 0;
        virtual BOOL SetDefaultTypeAttribute(DBGPROP_ATTRIB_FLAGS attributes) { return FALSE; };
        virtual WeakArenaReference<IDiagObjectModelWalkerBase>* CreateWalker() = 0;
        virtual BOOL IsLocalsAsRoot() { return FALSE; }
        virtual Var GetVarValue(BOOL fUpdated) { return nullptr; }
        virtual IDiagObjectAddress * GetDiagAddress() { return nullptr; }
        virtual DiagObjectModelDisplayType GetType() = 0;
        virtual bool IsFake() { return (this->GetTypeAttribute() & DBGPROP_ATTRIB_VALUE_IS_FAKE) == DBGPROP_ATTRIB_VALUE_IS_FAKE; }
        virtual bool IsLiteralProperty() const = 0;
        virtual bool IsSymbolProperty() { return false; }
    };

    //
    // There are three distinct types of classes defined in order to inspect a variable on watch/locals window.
    // If someone has to change or provide the support for custom types/objects (such as PixelArray etc) be displayed on the debugger, they need to aware
    // of few things which are mentioned below.

    // <...>Display (eg RecyclableArrayDisplay), mentions how current variable is given to debugger, and tells what walker (enumerator) to be chosen
    //              in order to walk to children of the current variable.
    // <...>Walker (eg RecyclableArrayWalker), mentions logic of walk thru content of the current variable (the object generally acts like an enumerator). Let say for an array, it has logic to go thru each
    //              indices and populate values.
    // <...>Address (eg RecyclableArrayAddress), associated with each child and will be used to updating that item. The object if this will be consumed by "<...>Walker"
    //              object when it walks thru each children of the current variable.

    // In order to support the custom objects, above classes should be used (or derived) to get started.
    //

    enum DebuggerPropertyDisplayInfoFlags
    {
        DebuggerPropertyDisplayInfoFlags_None           = 0x0,
        DebuggerPropertyDisplayInfoFlags_Const          = 0x1,
        DebuggerPropertyDisplayInfoFlags_InDeadZone     = 0x2,
        DebuggerPropertyDisplayInfoFlags_Unscope        = 0x4,
    };

    struct DebuggerPropertyDisplayInfo
    {
        PropertyId  propId;
        Var         aVar;
        DWORD       flags; // DebuggerPropertyDisplayInfoFlags.
        DebuggerPropertyDisplayInfo(PropertyId _propId, Var _aVar, DWORD _flags) : propId(_propId), aVar(_aVar), flags(_flags)
        {}

        bool IsUnscoped() const { return (flags & DebuggerPropertyDisplayInfoFlags_Unscope) != 0; }
        bool IsConst() const { return (flags & DebuggerPropertyDisplayInfoFlags_Const) != 0; }
        bool IsInDeadZone() const { return (flags & DebuggerPropertyDisplayInfoFlags_InDeadZone) != 0; }
    };

    enum UIGroupType
    {
        UIGroupType_None,
        UIGroupType_InnerScope,           // variables under the innerscope (such as Block/Catch)
        UIGroupType_Scope,
        UIGroupType_Globals
    };

    enum FramesLocalType
    {
        LocalType_None     = 0x0,
        LocalType_Reg      = 0x1,
        LocalType_InSlot   = 0x2,
        LocalType_InObject = 0x4,
    };

    enum FrameWalkerFlags
    {
        FW_None                 = 0x0,
        FW_MakeGroups           = 0x1,  // Make groups such as [Scope], [Globals] etc.
        FW_EnumWithScopeAlso    = 0x2,  // While walking include the with scope as well.
        FW_AllowLexicalThis     = 0x4,  // Do not filter out Js::PropertyIds::_lexicalThisSlotSymbol
        FW_AllowSuperReference  = 0x8,  // Allow walking of Js::PropertyIds::_superReferenceSymbol and Js::PropertyIds::_superCtorReferenceSymbol
    };

    class VariableWalkerBase : public IDiagObjectModelWalkerBase
    {
    public:

        DiagStackFrame*                                             pFrame;
        Var                                                         instance;

        JsUtil::List<DebuggerPropertyDisplayInfo*, ArenaAllocator>  *pMembersList;

        UIGroupType                                                 groupType;

    private:
        bool                                                        allowLexicalThis;
        bool                                                        allowSuperReference;

    public :

        VariableWalkerBase(DiagStackFrame* _pFrame, Var _instance, UIGroupType _groupType, bool allowLexicalThis, bool allowSuperReference = false)
            : pFrame(_pFrame), instance(_instance), pMembersList(nullptr), groupType(_groupType), allowLexicalThis(allowLexicalThis), allowSuperReference(allowSuperReference)
        {
        }

        // Defined virtual function, should be extended by type of variable scope.
        virtual void PopulateMembers() { };
        virtual IDiagObjectAddress * GetObjectAddress(int index) { return nullptr; }
        virtual Var GetVarObjectAt(int index);
        virtual bool IsConstAt(int index);

        /// IDiagObjectModelWalkerBase

        virtual BOOL Get(int i, ResolvedObject* pResolvedObject) override;
        virtual ulong GetChildrenCount() override;
        virtual BOOL GetGroupObject(ResolvedObject* pResolvedObject) override sealed;

        virtual IDiagObjectAddress *FindPropertyAddress(PropertyId propId, bool& isConst) override;

        static BOOL GetExceptionObject(int &index, DiagStackFrame* frame, ResolvedObject* pResolvedObject);
        static bool HasExceptionObject(DiagStackFrame* frame);

        static BOOL GetReturnedValue(int &index, DiagStackFrame* frame, ResolvedObject* pResolvedObject);
        static int  GetReturnedValueCount(DiagStackFrame* frame);

#ifdef ENABLE_MUTATION_BREAKPOINT
        static BOOL GetBreakMutationBreakpointValue(int &index, DiagStackFrame* frame, ResolvedObject* pResolvedObject);
        static uint GetBreakMutationBreakpointsCount(DiagStackFrame* frame);
#endif

        bool IsInGroup() const { return (groupType != UIGroupType::UIGroupType_None && groupType != UIGroupType::UIGroupType_InnerScope); }
        bool IsWalkerForCurrentFrame() const { return groupType == UIGroupType::UIGroupType_None; }

        int GetAdjustedByteCodeOffset() const;

        DebuggerPropertyDisplayInfo* AllocateNewPropertyDisplayInfo(PropertyId propertyId, Var value, bool isConst, bool isInDeadZone);

    protected:
        int GetMemberCount() { return pMembersList ? pMembersList->Count() : 0; }

        bool IsPropertyValid(PropertyId propertyId, RegSlot location, bool *isPropertyInDebuggerScope, bool* isConst, bool* isInDeadZone) const;
    };


    class RegSlotVariablesWalker : public VariableWalkerBase
    {
        // This will be pointing to the inner debugger scope (block/catch)
        DebuggerScope* debuggerScope;

    public:
        RegSlotVariablesWalker(DiagStackFrame* _pFrame, DebuggerScope *_debuggerScope, UIGroupType _groupType, bool allowSuperReference = false)
            : VariableWalkerBase(_pFrame, nullptr, _groupType, /* allowLexicalThis */ false, allowSuperReference), debuggerScope(_debuggerScope)
        {
        }

        virtual void PopulateMembers() override;
        virtual IDiagObjectAddress * GetObjectAddress(int index) override;
        virtual Var GetVarObjectAt(int index) override;

    private:
        bool IsRegisterValid(PropertyId propertyId, RegSlot registerSlot) const;
        bool IsRegisterInScope(PropertyId propertyId, RegSlot registerSlot) const;
        Var GetVarObjectAndRegAt(int index, RegSlot* reg = nullptr);
    };

    class SlotArrayVariablesWalker : public VariableWalkerBase
    {
    public:
        SlotArrayVariablesWalker(DiagStackFrame* _pFrame, Var _instance, UIGroupType _groupType, bool allowLexicalThis, bool allowSuperReference = false) : VariableWalkerBase(_pFrame, _instance, _groupType, allowLexicalThis, allowSuperReference) {}

        virtual void PopulateMembers() override;
        virtual IDiagObjectAddress * GetObjectAddress(int index) override;

        ScopeSlots GetSlotArray() {
            Var *slotArray = (Var *) instance;
            Assert(slotArray != nullptr);
            return ScopeSlots(slotArray);
        }
    };

    class ObjectVariablesWalker : public VariableWalkerBase
    {
    public:
        ObjectVariablesWalker(DiagStackFrame* _pFrame, Var _instance, UIGroupType _groupType, bool allowLexicalThis, bool allowSuperReference = false) : VariableWalkerBase(_pFrame, _instance, _groupType, allowLexicalThis, allowSuperReference) {}

        virtual void PopulateMembers() override;
        virtual IDiagObjectAddress * GetObjectAddress(int index) override;

    protected:
        void AddObjectProperties(int count, Js::RecyclableObject* object);
    };

    class RootObjectVariablesWalker : public ObjectVariablesWalker
    {
    public:
        RootObjectVariablesWalker(DiagStackFrame* _pFrame, Var _instance, UIGroupType _groupType) : ObjectVariablesWalker(_pFrame, _instance, _groupType, /* allowLexicalThis */ false) {}

        virtual void PopulateMembers() override;
    };

    class DiagScopeVariablesWalker sealed : public VariableWalkerBase
    {
    public:
        // Represent catch/with scope objects, (ie. the representation for the diagnostics purposes.)
        JsUtil::List<IDiagObjectModelWalkerBase*, ArenaAllocator> *pDiagScopeObjects;
        ulong diagScopeVarCount;

        bool scopeIsInitialized;
        bool enumWithScopeAlso;

    public:
        DiagScopeVariablesWalker(DiagStackFrame* _pFrame, Var _instance, bool _enumWithScopeAlso)
            : VariableWalkerBase(_pFrame, _instance, UIGroupType_InnerScope, /* allowLexicalThis */ false), pDiagScopeObjects(nullptr), diagScopeVarCount(0), scopeIsInitialized(false), enumWithScopeAlso(_enumWithScopeAlso)
        {}

        DiagScopeVariablesWalker(DiagStackFrame* _pFrame, Var _instance, IDiagObjectModelWalkerBase* innerWalker);

        virtual BOOL Get(int i, ResolvedObject* pResolvedObject) override;
        virtual ulong GetChildrenCount() override;

        virtual IDiagObjectAddress *FindPropertyAddress(PropertyId propId, bool& isConst) override;
    };


    // Display of variable on the locals window
    // Also responsible for walking on the current frame and build up chain of scopes.
    class LocalsWalker sealed : public IDiagObjectModelWalkerBase
    {
        friend class RecyclableArgumentsArrayWalker;

        DiagStackFrame* pFrame;

        JsUtil::List<VariableWalkerBase *, ArenaAllocator> * pVarWalkers; // This includes, current frame, all scopes and globals for a current frame
        uint totalLocalsCount;
        DWORD frameWalkerFlags;

        // true, if user has not defined the 'arguments' in the script, this is used for displaying a fake arguments object and display in the locals window.
        bool hasUserNotDefinedArguments;

    public:
        LocalsWalker(DiagStackFrame* _frame, DWORD _frameWalkerFlags);
        virtual BOOL Get(int i, ResolvedObject* pResolvedObject) override;
        virtual ulong GetChildrenCount() override;

        virtual BOOL GetGroupObject(ResolvedObject* pResolvedObject) {return FALSE; }

        static DWORD GetCurrentFramesLocalsType(DiagStackFrame* frame);

        IDiagObjectAddress * FindPropertyAddress(PropertyId propId, bool& isConst) override;

        // enumerateGroups will be true for the when fetching from a variable from the expression evaluation.
        IDiagObjectAddress * FindPropertyAddress(PropertyId propId, bool enumerateGroups, bool& isConst);

        template <typename FnProcessResolvedObject>
        DynamicObject* CreateAndPopulateActivationObject(ScriptContext* scriptContext, FnProcessResolvedObject processResolvedObjectFn)
        {
            Assert(scriptContext);
            Js::DynamicObject* activeScopeObject = nullptr;
            ulong count = this->GetChildrenCount();
            if (count > 0)
            {
                activeScopeObject = scriptContext->GetLibrary()->CreateActivationObject();
                for (ulong i = 0; i < count; i++)
                {
                    Js::ResolvedObject resolveObject;
                    if (this->Get(i, &resolveObject) && resolveObject.propId != Js::Constants::NoProperty)
                    {
                        if (!activeScopeObject->HasOwnProperty(resolveObject.propId))
                        {
                            OUTPUT_TRACE(Js::ConsoleScopePhase, L"Adding '%s' property to activeScopeObject\n", resolveObject.scriptContext->GetPropertyName(resolveObject.propId)->GetBuffer());
                            if (resolveObject.IsInDeadZone())
                            {
                                PropertyOperationFlags flags = static_cast<PropertyOperationFlags>(PropertyOperation_SpecialValue | PropertyOperation_AllowUndecl);
                                PropertyAttributes attributes = resolveObject.isConst ? PropertyConstDefaults : PropertyLetDefaults;
                                resolveObject.obj = scriptContext->GetLibrary()->GetUndeclBlockVar();
                                activeScopeObject->SetPropertyWithAttributes(
                                        resolveObject.propId,
                                        resolveObject.obj,
                                        attributes, nullptr, flags);
                            }
                            else
                            {
                                activeScopeObject->SetPropertyWithAttributes(
                                    resolveObject.propId,
                                    JavascriptOperators::BoxStackInstance(resolveObject.obj, scriptContext), //The value escapes, box if necessary.
                                    resolveObject.isConst ? PropertyConstDefaults : PropertyDynamicTypeDefaults,
                                    nullptr);
                            }
                            processResolvedObjectFn(resolveObject);
                        }
                    }
                }
            }
            return activeScopeObject;
        }

    private:
        BOOL CreateArgumentsObject(ResolvedObject* pResolvedObject);
        bool ShouldMakeGroups() const { return frameWalkerFlags & FW_MakeGroups; }
        bool ShouldInsertFakeArguments();
        void ExpandArgumentsObject(IDiagObjectModelDisplay * argumentsDisplay);
    };

    class LocalsDisplay : public IDiagObjectModelDisplay
    {
        DiagStackFrame* pFrame;

    public:
        LocalsDisplay(DiagStackFrame* _frame);

        virtual LPCWSTR Name() override;
        virtual LPCWSTR Type() override;
        virtual LPCWSTR Value(int radix) override;
        virtual BOOL HasChildren() override;
        virtual BOOL Set(Var updateObject) override;
        virtual DBGPROP_ATTRIB_FLAGS GetTypeAttribute() override;
        virtual WeakArenaReference<IDiagObjectModelWalkerBase>* CreateWalker() override;
        virtual BOOL IsLocalsAsRoot() { return TRUE; }
        virtual DiagObjectModelDisplayType GetType() { return DiagObjectModelDisplayType_LocalsDisplay; }
        virtual bool IsLiteralProperty() const { return false; }
    };

    //
    // The locals var's addresses.


    // A representation of a address when this Var is taken from the slot array.
    class LocalObjectAddressForSlot : public IDiagObjectAddress
    {
        ScopeSlots slotArray;
        int slotIndex;
        Var value;

    public:
        LocalObjectAddressForSlot(ScopeSlots _pSlotArray, int _slotIndex, Js::Var _value);

        virtual BOOL Set(Var updateObject) override;
        virtual Var GetValue(BOOL fUpdated);
        virtual BOOL IsInDeadZone() const;
    };

    // A representation of a address when this Var is taken from the direct regslot.
    class LocalObjectAddressForRegSlot : public IDiagObjectAddress
    {
        DiagStackFrame* pFrame;
        RegSlot regSlot;
        Var value;

    public:
        LocalObjectAddressForRegSlot(DiagStackFrame* _pFrame, RegSlot _regSlot, Js::Var _value);

        virtual BOOL Set(Var updateObject) override;
        virtual Var GetValue(BOOL fUpdated);
        BOOL IsInDeadZone() const;
    };

    class CatchScopeWalker sealed : public IDiagObjectModelWalkerBase
    {
        DiagStackFrame* pFrame;
        DebuggerScope * debuggerScope;
    public :

        CatchScopeWalker(DiagStackFrame* _pFrame, DebuggerScope* _debuggerScope)
            : pFrame(_pFrame), debuggerScope(_debuggerScope)
        {
        }

        /// IDiagObjectModelWalkerBase

        virtual BOOL Get(int i, ResolvedObject* pResolvedObject) override;
        virtual ulong GetChildrenCount() override;
        virtual BOOL GetGroupObject(ResolvedObject* pResolvedObject) override { return FALSE; }
        virtual IDiagObjectAddress *FindPropertyAddress(PropertyId propId, bool& isConst) override;
    private:
        void FetchValueAndAddress(DebuggerScopeProperty &scopeProperty, _Out_opt_ Var *pValue, _Out_opt_ IDiagObjectAddress ** ppAddress);
    };


    // Concrete Classes for Objects

    class RecyclableObjectWalker : public IDiagObjectModelWalkerBase
    {
    protected:
        ScriptContext* scriptContext;
        Var instance;
        Var originalInstance;             // Remember original instance for prototype walk, because evaluating getters in CallGetter() if __proto__ instance is passed does not work
        JsUtil::List<DebuggerPropertyDisplayInfo *, ArenaAllocator> * pMembersList;

        RecyclableArrayWalker * innerArrayObjectWalker;                 // Will be used for array indices on the object
        JsUtil::List<IDiagObjectModelWalkerBase *, ArenaAllocator> * fakeGroupObjectWalkerList; // such as [prototype], [Methods] etc.

        void InsertItem(Js::RecyclableObject *pOriginalObject, Js::RecyclableObject *pObject, PropertyId propertyId, bool isConst, bool isUnscoped, Js::RecyclableMethodsGroupWalker **ppMethodsGrouptWalker, bool shouldPinProperty = false);
        void InsertItem(PropertyId propertyId, bool isConst, bool isUnscoped, Var itemObj, Js::RecyclableMethodsGroupWalker **ppMethodsGrouptWalker, bool shouldPinProperty = false);

        void EnsureFakeGroupObjectWalkerList();

    public:
        RecyclableObjectWalker(ScriptContext* pContext, Var slot);
        RecyclableObjectWalker(ScriptContext* pContext, Var slot, Var originalInstance);

        virtual BOOL Get(int i, ResolvedObject* pResolvedObject) override;
        virtual ulong GetChildrenCount() override;

        virtual BOOL GetGroupObject(ResolvedObject* pResolvedObject)  { return FALSE; };
        virtual IDiagObjectAddress *FindPropertyAddress(PropertyId propertyId, bool& isConst) override;

        static Var GetObject(RecyclableObject* originalInstance, RecyclableObject* instance, PropertyId propertyId, ScriptContext* scriptContext);
    };


    class RecyclableObjectAddress : public IDiagObjectAddress
    {
        Var parentObj;
        Js::PropertyId propId;
        Js::Var value;
        BOOL isInDeadZone;

    public:
        RecyclableObjectAddress(Var parentObj, Js::PropertyId _propId, Js::Var _value, BOOL _isInDeadZone);
        virtual BOOL Set(Var updateObject) override;
        virtual BOOL IsWritable() override;
        virtual Var GetValue(BOOL fUpdated);
        BOOL IsInDeadZone() const;
    };

    class RecyclableObjectDisplay : public IDiagObjectModelDisplay
    {
    protected:
        ScriptContext* scriptContext;
        Var instance;
        Var originalInstance;
        LPCWSTR name;
        IDiagObjectAddress* pObjAddress;
        DBGPROP_ATTRIB_FLAGS defaultAttributes;
        PropertyId propertyId;

    public:
        RecyclableObjectDisplay(ResolvedObject* resolvedObject, DBGPROP_ATTRIB_FLAGS defaultAttributes = DBGPROP_ATTRIB_NO_ATTRIB);

        virtual LPCWSTR Name() override;
        virtual LPCWSTR Type() override;
        virtual LPCWSTR Value(int radix) override;
        virtual BOOL HasChildren() override;
        virtual BOOL Set(Var updateObject) override;
        virtual BOOL SetDefaultTypeAttribute(DBGPROP_ATTRIB_FLAGS attributes) override { defaultAttributes = attributes; return TRUE; };
        virtual DBGPROP_ATTRIB_FLAGS GetTypeAttribute() override;
        virtual WeakArenaReference<IDiagObjectModelWalkerBase>* CreateWalker() override;
        virtual Var GetVarValue(BOOL fUpdated) override;
        virtual IDiagObjectAddress * GetDiagAddress() override { return pObjAddress; }
        virtual DiagObjectModelDisplayType GetType() { return DiagObjectModelDisplayType_RecyclableObjectDisplay; }
        virtual bool IsLiteralProperty() const;
        virtual bool IsSymbolProperty() override;

        static BOOL GetPropertyWithScriptEnter(RecyclableObject* originalInstance, RecyclableObject* instance, PropertyId propertyId, Var* value, ScriptContext* scriptContext);
        StringBuilder<ArenaAllocator>* GetStringBuilder();

        PropertyId GetPropertyId() const;
    };


    // Concrete classes for Arrays

    class RecyclableArrayAddress : public IDiagObjectAddress
    {
    protected:
        Var parentArray;
        unsigned int index;

    public:
        RecyclableArrayAddress(Var parentArray, unsigned int index);
        virtual BOOL Set(Var updateObject) override;
    };

    class RecyclableArrayDisplay : public RecyclableObjectDisplay
    {
    protected:
        BOOL HasChildrenInternal(Js::JavascriptArray* arrayObj);

    public:
        RecyclableArrayDisplay(ResolvedObject* resolvedObject);

        virtual BOOL HasChildren() override;
        virtual WeakArenaReference<IDiagObjectModelWalkerBase>* CreateWalker() override;
    };

    class RecyclableArrayWalker : public RecyclableObjectWalker
    {
    protected:
        uint32 indexedItemCount;
        JsUtil::List<uint32, ArenaAllocator> * pAbsoluteIndexList;

        // Just populate the indexes only.
        bool fOnlyOwnProperties;

        uint32 RecyclableArrayWalker::GetItemCount(Js::JavascriptArray* arrayObj);

        // ES5Array will extend this.
        virtual uint32 GetNextDescriptor(uint32 currentDescriptor) { return Js::JavascriptArray::InvalidIndex; }

        LPCWSTR RecyclableArrayWalker::GetIndexName(uint32 index, StringBuilder<ArenaAllocator>* stringBuilder);

        Js::JavascriptArray* GetArrayObject();

    public:
        RecyclableArrayWalker(ScriptContext* pContext, Var slot, Var originalInstance);
        void SetOnlyWalkOwnProperties(bool set) { fOnlyOwnProperties = set; }

        virtual BOOL Get(int i, ResolvedObject* pResolvedObject) override;
        virtual ulong GetChildrenCount() override;
        virtual BOOL FetchItemAtIndex(Js::JavascriptArray* arrayObj, uint32 index, Var *value);
        virtual Var FetchItemAt(Js::JavascriptArray* arrayObj, uint32 index);
        virtual BOOL GetResolvedObject(Js::JavascriptArray* arrayObj, int index, ResolvedObject* pResolvedObject, uint32 * pabsIndex) sealed;

        StringBuilder<ArenaAllocator>* GetBuilder();
    };

    // Concrete classes for Arguments object
    //

    class RecyclableArgumentsObjectDisplay : public RecyclableObjectDisplay
    {
        LocalsWalker *pLocalsWalker;
    public:
        RecyclableArgumentsObjectDisplay(ResolvedObject* resolvedObject, LocalsWalker *localsWalker);

        virtual BOOL HasChildren() override;
        virtual WeakArenaReference<IDiagObjectModelWalkerBase>* CreateWalker() override;
    };

    class RecyclableArgumentsObjectWalker : public RecyclableObjectWalker
    {
        LocalsWalker *pLocalsWalker;
    public:
        RecyclableArgumentsObjectWalker(ScriptContext* pContext, Var instance, LocalsWalker * localsWalker);

        virtual ulong GetChildrenCount() override;
    };

    class RecyclableArgumentsArrayAddress : public IDiagObjectAddress
    {
        Var parentArray;
        unsigned int index;

    public:
        RecyclableArgumentsArrayAddress(Var parentArray, unsigned int index);
        virtual BOOL Set(Var updateObject) override;
    };

    class RecyclableArgumentsArrayWalker : public RecyclableArrayWalker
    {
        JsUtil::List<IDiagObjectAddress *, ArenaAllocator> * pFormalsList;

    public:
        RecyclableArgumentsArrayWalker(ScriptContext* pContext, Var slot, Var originalInstance);

        virtual BOOL Get(int i, ResolvedObject* pResolvedObject) override;
        virtual ulong GetChildrenCount() override;

        void FetchFormalsAddress (LocalsWalker * localsWalker);
    };

    // Concrete classes for Typed array objects
    //

    class RecyclableTypedArrayAddress : public RecyclableArrayAddress
    {
    public:
        RecyclableTypedArrayAddress(Var parentArray, unsigned int index);
        virtual BOOL Set(Var updateObject) override;
    };

    class RecyclableTypedArrayDisplay : public RecyclableObjectDisplay
    {
    public:
        RecyclableTypedArrayDisplay(ResolvedObject* resolvedObject);

        virtual BOOL HasChildren() override;
        virtual WeakArenaReference<IDiagObjectModelWalkerBase>* CreateWalker() override;
    };

    class RecyclableTypedArrayWalker : public RecyclableArrayWalker
    {
    public:
        RecyclableTypedArrayWalker(ScriptContext* pContext, Var slot, Var originalInstance);

        virtual BOOL Get(int i, ResolvedObject* pResolvedObject) override;
        virtual ulong GetChildrenCount() override;
    };

    // Concrete classes for Pixel array objects
    //

    class RecyclablePixelArrayAddress : public RecyclableArrayAddress
    {
    public:
        RecyclablePixelArrayAddress(Var parentArray, unsigned int index);
        virtual BOOL Set(Var updateObject) override;
    };

    class RecyclablePixelArrayDisplay : public RecyclableObjectDisplay
    {
    public:
        RecyclablePixelArrayDisplay(ResolvedObject* resolvedObject);

        virtual BOOL HasChildren() override;
        virtual WeakArenaReference<IDiagObjectModelWalkerBase>* CreateWalker() override;
    };

    class RecyclablePixelArrayWalker : public RecyclableArrayWalker
    {
    public:
        RecyclablePixelArrayWalker(ScriptContext* pContext, Var slot, Var originalInstance);

        virtual BOOL Get(int i, ResolvedObject* pResolvedObject) override;
        virtual ulong GetChildrenCount() override;
    };

    // Concrete classes for ES5 array objects
    //

    class RecyclableES5ArrayAddress : public RecyclableArrayAddress
    {
    public:
        RecyclableES5ArrayAddress(Var parentArray, unsigned int index);
        virtual BOOL Set(Var updateObject) override;
    };

    class RecyclableES5ArrayDisplay : public RecyclableArrayDisplay
    {
    public:
        RecyclableES5ArrayDisplay(ResolvedObject* resolvedObject);

        virtual BOOL HasChildren() override;
        virtual WeakArenaReference<IDiagObjectModelWalkerBase>* CreateWalker() override;
    };

    class RecyclableES5ArrayWalker sealed : public RecyclableArrayWalker
    {
    public:
        RecyclableES5ArrayWalker(ScriptContext* pContext, Var slot, Var originalInstance);

        virtual uint32 GetNextDescriptor(uint32 currentDescriptor) override;

        virtual BOOL FetchItemAtIndex(Js::JavascriptArray* arrayObj, uint32 index, Var *value) override;
        virtual Var FetchItemAt(Js::JavascriptArray* arrayObj, uint32 index) override;
    };

    // Concrete classes for Proto group object
    //

    class RecyclableProtoObjectWalker : public RecyclableObjectWalker
    {
    public:
        RecyclableProtoObjectWalker(ScriptContext* pContext, Var slot, Var originalInstance);
        virtual BOOL GetGroupObject(ResolvedObject* pResolvedObject) override;
        virtual IDiagObjectAddress *FindPropertyAddress(PropertyId propId, bool& isConst) override;
    };

    class RecyclableProtoObjectAddress : public RecyclableObjectAddress
    {
    public:
        RecyclableProtoObjectAddress(Var _parentObj, Js::PropertyId _propId, Js::Var _value);
    };


    // Concrete classes for Map, Set, and WeakMap group objects
    //

    template <typename TData>
    struct RecyclableCollectionObjectWalkerPropertyData
    {
        RecyclableCollectionObjectWalkerPropertyData():key(nullptr), value(nullptr) { }
        RecyclableCollectionObjectWalkerPropertyData(Var key, Var value):key(key), value(value) { }
        Var key;
        Var value;
    };

    template<>
    struct RecyclableCollectionObjectWalkerPropertyData<JavascriptSet>
    {
        RecyclableCollectionObjectWalkerPropertyData():value(nullptr) { }
        RecyclableCollectionObjectWalkerPropertyData(Var value):value(value) { }
        Var value;
    };

    template<>
    struct RecyclableCollectionObjectWalkerPropertyData<JavascriptWeakSet>
    {
        RecyclableCollectionObjectWalkerPropertyData():value(nullptr) { }
        RecyclableCollectionObjectWalkerPropertyData(Var value):value(value) { }
        Var value;
    };

    template <typename TData>
    class RecyclableCollectionObjectWalker : public IDiagObjectModelWalkerBase
    {
        ScriptContext* scriptContext;
        Var instance;

        JsUtil::List<RecyclableCollectionObjectWalkerPropertyData<TData>, ArenaAllocator>* propertyList;

        const wchar_t* Name();
        IDiagObjectModelDisplay* CreateTDataDisplay(ResolvedObject* resolvedObject, int i);
        void GetChildren();

    public:
        RecyclableCollectionObjectWalker(ScriptContext* scriptContext, Var instance):scriptContext(scriptContext), instance(instance), propertyList(nullptr) { }
        virtual BOOL GetGroupObject(ResolvedObject* pResolvedObject) override;
        virtual BOOL Get(int i, ResolvedObject* pResolvedObject) override;
        virtual ulong GetChildrenCount() override;
    };

    typedef RecyclableCollectionObjectWalker<JavascriptMap> RecyclableMapObjectWalker;
    typedef RecyclableCollectionObjectWalker<JavascriptSet> RecyclableSetObjectWalker;
    typedef RecyclableCollectionObjectWalker<JavascriptWeakMap> RecyclableWeakMapObjectWalker;
    typedef RecyclableCollectionObjectWalker<JavascriptWeakSet> RecyclableWeakSetObjectWalker;

    template <typename TData>
    class RecyclableCollectionObjectDisplay : public IDiagObjectModelDisplay
    {
        ScriptContext* scriptContext;
        const wchar_t* name;
        RecyclableCollectionObjectWalker<TData>* walker;

    public:
        RecyclableCollectionObjectDisplay(ScriptContext* scriptContext, const wchar_t* name, RecyclableCollectionObjectWalker<TData>* walker) : scriptContext(scriptContext), name(name), walker(walker) { }

        virtual LPCWSTR Name() override { return name; }
        virtual LPCWSTR Type() override { return L""; }
        virtual LPCWSTR Value(int radix) override;
        virtual BOOL HasChildren() override { return walker->GetChildrenCount() > 0; }
        virtual BOOL Set(Var updateObject) override { return FALSE; }
        virtual BOOL SetDefaultTypeAttribute(DBGPROP_ATTRIB_FLAGS attributes) override { return FALSE; }
        virtual DBGPROP_ATTRIB_FLAGS GetTypeAttribute() override { return DBGPROP_ATTRIB_VALUE_READONLY | DBGPROP_ATTRIB_VALUE_IS_FAKE | (HasChildren() ? DBGPROP_ATTRIB_VALUE_IS_EXPANDABLE : 0); }
        virtual WeakArenaReference<IDiagObjectModelWalkerBase>* CreateWalker() override;
        virtual Var GetVarValue(BOOL fUpdated) override { return nullptr; }
        virtual IDiagObjectAddress * GetDiagAddress() override { return nullptr; }
        virtual DiagObjectModelDisplayType GetType() { return DiagObjectModelDisplayType_RecyclableCollectionObjectDisplay; }
        virtual bool IsLiteralProperty() const { return false; }
    };

    class RecyclableKeyValueDisplay : public IDiagObjectModelDisplay
    {
        ScriptContext* scriptContext;
        Var key;
        Var value;
        const wchar_t* name;

    public:
        RecyclableKeyValueDisplay(ScriptContext* scriptContext, Var key, Var value, const wchar_t* name) : scriptContext(scriptContext), key(key), value(value), name(name) { }

        virtual LPCWSTR Name() override { return name; }
        virtual LPCWSTR Type() override { return L""; }
        virtual LPCWSTR Value(int radix) override;
        virtual BOOL HasChildren() override { return TRUE; }
        virtual BOOL Set(Var updateObject) override { return FALSE; }
        virtual DBGPROP_ATTRIB_FLAGS GetTypeAttribute() override { return DBGPROP_ATTRIB_VALUE_IS_EXPANDABLE | DBGPROP_ATTRIB_VALUE_IS_FAKE | DBGPROP_ATTRIB_VALUE_READONLY; }
        virtual WeakArenaReference<IDiagObjectModelWalkerBase>* CreateWalker() override;
        virtual DiagObjectModelDisplayType GetType() { return DiagObjectModelDisplayType_RecyclableKeyValueDisplay; }
        virtual bool IsLiteralProperty() const { return false; }
    };

    class RecyclableKeyValueWalker : public IDiagObjectModelWalkerBase
    {
        ScriptContext* scriptContext;
        Var key;
        Var value;

    public:
        RecyclableKeyValueWalker(ScriptContext* scriptContext, Var key, Var value):scriptContext(scriptContext), key(key), value(value) { }

        virtual BOOL Get(int i, ResolvedObject* pResolvedObject) override;
        virtual ulong GetChildrenCount() override { return 2; }
        virtual BOOL GetGroupObject(ResolvedObject* pResolvedObject) override { return FALSE; }
    };

    class RecyclableProxyObjectDisplay : public RecyclableObjectDisplay
    {
    public:
        RecyclableProxyObjectDisplay(ResolvedObject* resolvedObject);

        virtual BOOL HasChildren() override { return TRUE; }
        virtual WeakArenaReference<IDiagObjectModelWalkerBase>* CreateWalker() override;
    };

    class RecyclableProxyObjectWalker : public RecyclableObjectWalker
    {
    public:
        RecyclableProxyObjectWalker(ScriptContext* pContext, Var instance);

        virtual BOOL Get(int i, ResolvedObject* pResolvedObject) override;
        virtual ulong GetChildrenCount() override { return 2; }
        virtual BOOL GetGroupObject(ResolvedObject* pResolvedObject) override;
    };

    // Concrete classes for Methods group object
    //

    class RecyclableMethodsGroupWalker : public RecyclableObjectWalker
    {
    public:
        RecyclableMethodsGroupWalker(ScriptContext* pContext, Var slot);
        void AddItem(Js::PropertyId propertyId, Var obj);

        virtual BOOL Get(int i, ResolvedObject* pResolvedObject) override;
        virtual ulong GetChildrenCount() override;
        virtual BOOL GetGroupObject(ResolvedObject* pResolvedObject) override;

        void Sort();
    };

    class RecyclableMethodsGroupDisplay : public RecyclableObjectDisplay
    {
    public:
        RecyclableMethodsGroupWalker *methodGroupWalker;

        RecyclableMethodsGroupDisplay(RecyclableMethodsGroupWalker *_methodGroupWalker, ResolvedObject* resolvedObject);

        virtual LPCWSTR Type() override;
        virtual LPCWSTR Value(int radix) override;
        virtual BOOL HasChildren() override;
        virtual DBGPROP_ATTRIB_FLAGS GetTypeAttribute() override;
        virtual WeakArenaReference<IDiagObjectModelWalkerBase>* CreateWalker() override;
    };

    // Concrete classes for Scope group object
    //


    class ScopeVariablesGroupDisplay : public RecyclableObjectDisplay
    {
    public:
        VariableWalkerBase *scopeGroupWalker;

        ScopeVariablesGroupDisplay(VariableWalkerBase *walker, ResolvedObject* resolvedObject);

        virtual LPCWSTR Type() override;
        virtual LPCWSTR Value(int radix) override;
        virtual BOOL HasChildren() override;
        virtual DBGPROP_ATTRIB_FLAGS GetTypeAttribute() override;
        virtual WeakArenaReference<IDiagObjectModelWalkerBase>* CreateWalker() override;
    };

    // Concrete classes for Globals group object
    //

    class GlobalsScopeVariablesGroupDisplay sealed : public RecyclableObjectDisplay
    {
    public:
        VariableWalkerBase *globalsGroupWalker;

        GlobalsScopeVariablesGroupDisplay(VariableWalkerBase *walker, ResolvedObject* resolvedObject);

        virtual LPCWSTR Type() override;
        virtual LPCWSTR Value(int radix) override;
        virtual BOOL HasChildren() override;
        virtual DBGPROP_ATTRIB_FLAGS GetTypeAttribute() override;
        virtual WeakArenaReference<IDiagObjectModelWalkerBase>* CreateWalker() override;
    };

#ifdef ENABLE_MUTATION_BREAKPOINT
    // For Pending Mutation breakpoint

    class PendingMutationBreakpointDisplay : public RecyclableObjectDisplay
    {
        MutationType mutationType;
    public:
        PendingMutationBreakpointDisplay(ResolvedObject* resolvedObject, MutationType mutationType);
        virtual LPCWSTR Value(int radix) override { return L""; }
        virtual BOOL HasChildren() override { return TRUE; }
        virtual WeakArenaReference<IDiagObjectModelWalkerBase>* CreateWalker() override;
    };

    class PendingMutationBreakpointWalker : public RecyclableObjectWalker
    {
        MutationType mutationType;
    public:
        PendingMutationBreakpointWalker(ScriptContext* pContext, Var instance, MutationType mutationType);

        virtual BOOL Get(int i, ResolvedObject* pResolvedObject) override;
        virtual ulong GetChildrenCount() override;
    };
#endif
}
