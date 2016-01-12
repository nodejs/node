//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once


namespace Js
{
    class JavascriptRegExpConstructor : public RuntimeFunction
    {
        friend class RegexHelper;
    private:
        static PropertyId const specialPropertyIds[];
        static PropertyId const specialnonEnumPropertyIds[];
        static PropertyId const specialEnumPropertyIds[];
        static const int NumCtorCaptures = 10;

        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(JavascriptRegExpConstructor);
    protected:
        //To prevent lastMatch from being cleared from cross-site marshalling
        DEFINE_VTABLE_CTOR_MEMBER_INIT(JavascriptRegExpConstructor, RuntimeFunction, lastMatch);

    public:
        JavascriptRegExpConstructor(DynamicType * type);

        virtual BOOL HasProperty(PropertyId propertyId) override;
        virtual BOOL GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext) override;
        virtual BOOL GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext) override;
        virtual BOOL GetPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext) override;
        virtual BOOL SetProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info) override;
        virtual BOOL SetProperty(JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info) override;
        virtual BOOL InitProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags = PropertyOperation_None, PropertyValueInfo* info = NULL) override;
        virtual BOOL DeleteProperty(PropertyId propertyId, PropertyOperationFlags flags) override;
        virtual BOOL GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext) override;
        virtual BOOL GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext) override;
        virtual BOOL IsEnumerable(PropertyId propertyId) override;
        virtual BOOL IsConfigurable(PropertyId propertyId) override;
        virtual BOOL GetEnumerator(BOOL enumNonEnumerable, Var* enumerator, ScriptContext * requestContext, bool preferSnapshotSemantics = true, bool enumSymbols = false) override;
        BOOL GetSpecialNonEnumerablePropertyName(uint32 index, Var *propertyName, ScriptContext * requestContext);
        uint GetSpecialNonEnumerablePropertyCount() const;
        PropertyId const * GetSpecialNonEnumerablePropertyIds() const;
        BOOL GetSpecialEnumerablePropertyName(uint32 index, Var *propertyName, ScriptContext * requestContext);
        uint GetSpecialEnumerablePropertyCount() const;
        PropertyId const * GetSpecialEnumerablePropertyIds() const;
        virtual BOOL GetSpecialPropertyName(uint32 index, Var *propertyName, ScriptContext * requestContext) override;
        virtual uint GetSpecialPropertyCount() const override;
        virtual PropertyId const * GetSpecialPropertyIds() const override;
        UnifiedRegex::RegexPattern* GetLastPattern() const { return lastPattern; }

    private:
        bool GetPropertyBuiltIns(PropertyId propertyId, Var* value, BOOL* result);
        bool SetPropertyBuiltIns(PropertyId propertyId, Var value, BOOL* result);
        void SetLastMatch(UnifiedRegex::RegexPattern* lastPattern, JavascriptString* lastInput, UnifiedRegex::GroupInfo lastMatch);

        void EnsureValues();

        UnifiedRegex::RegexPattern* lastPattern;
        JavascriptString* lastInput;
        UnifiedRegex::GroupInfo lastMatch;
        bool reset; // true if following fields must be recalculated from above before first use
        Var lastParen;
        Var lastIndex;
        Var index;
        Var leftContext;
        Var rightContext;
        Var captures[NumCtorCaptures];
    };

    class JavascriptRegExpConstructorProperties
    {
    public:
        static bool IsSpecialProperty(PropertyId id)
        {
            switch (id)
            {
                case PropertyIds::input:
                case PropertyIds::$_:

                case PropertyIds::lastMatch:
                case PropertyIds::$Ampersand:

                case PropertyIds::lastParen:
                case PropertyIds::$Plus:

                case PropertyIds::leftContext:
                case PropertyIds::$BackTick:

                case PropertyIds::rightContext:
                case PropertyIds::$Tick:

                case PropertyIds::$1:
                case PropertyIds::$2:
                case PropertyIds::$3:
                case PropertyIds::$4:
                case PropertyIds::$5:
                case PropertyIds::$6:
                case PropertyIds::$7:
                case PropertyIds::$8:
                case PropertyIds::$9:
                case PropertyIds::index:
                    return true;
            }
            return false;
        }
    };

} // namespace Js
