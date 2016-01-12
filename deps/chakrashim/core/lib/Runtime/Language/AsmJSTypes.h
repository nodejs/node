//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
// Portions of this file are copyright 2014 Mozilla Foundation, available under the Apache 2.0 license.
//-------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------
// Copyright 2014 Mozilla Foundation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http ://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//-------------------------------------------------------------------------------------------------------

#pragma once

#ifndef TEMP_DISABLE_ASMJS
namespace Js
{
    typedef uint32 uint32_t;
    typedef IdentPtr PropertyName;
    typedef ParseNode* AsmJSParser;

    // These EcmaScript-defined coercions form the basis of the asm.js type system.
    enum AsmJSCoercion
    {
        AsmJS_ToInt32,
        AsmJS_ToNumber,
        AsmJS_FRound,
        AsmJS_Int32x4,
        AsmJS_Float32x4,
        AsmJS_Float64x2,
    };



    namespace ArrayBufferView
    {

        enum ViewType
        {
            TYPE_INT8 = 0,
            TYPE_UINT8,
            TYPE_INT16,
            TYPE_UINT16,
            TYPE_INT32,
            TYPE_UINT32,
            TYPE_FLOAT32,
            TYPE_FLOAT64,
            TYPE_INVALID
        };

    } /* namespace ArrayBufferView */
    // The asm.js spec recognizes this set of builtin Math functions.
    enum AsmJSMathBuiltinFunction
    {
#define ASMJS_MATH_FUNC_NAMES(name, propertyName) AsmJSMathBuiltin_##name,
#include "AsmJsBuiltinNames.h"
        AsmJSMathBuiltinFunction_COUNT,
#define ASMJS_MATH_CONST_NAMES(name, propertyName) AsmJSMathBuiltin_##name,
#include "AsmJsBuiltinNames.h"
        AsmJSMathBuiltin_COUNT
    };
    enum AsmJSTypedArrayBuiltinFunction
    {
#define ASMJS_ARRAY_NAMES(name, propertyName) AsmJSTypedArrayBuiltin_##name,
#include "AsmJsBuiltinNames.h"
        AsmJSTypedArrayBuiltin_COUNT
    };
    // Represents the type of a general asm.js expression.
    class AsmJsType
    {
    public:
        enum Which
        {
            Int,
            Double,
            Float,
            MaybeDouble,
            DoubleLit,          // Double literal. Needed for SIMD.js. Sub-type of Double
            MaybeFloat,
            Floatish,
            FloatishDoubleLit,  // A sum-type for Floatish and DoubleLit. Needed for float32x4(..) arg types.
            Fixnum,
            Signed,
            Unsigned,
            Intish,
            Void,
            Int32x4,
            Float32x4,
            Float64x2
        };

    private:
        Which which_;

    public:
        AsmJsType() : which_( Which( -1 ) ){}
        AsmJsType( Which w ) : which_( w ){}

        bool operator==( AsmJsType rhs ) const;
        bool operator!=( AsmJsType rhs ) const;
        inline Js::AsmJsType::Which GetWhich() const{return which_;}
        bool isSigned() const;
        bool isUnsigned() const;
        bool isInt() const;
        bool isIntish() const;
        bool isDouble() const;
        bool isMaybeDouble() const;
        bool isDoubleLit() const;
        bool isFloat() const;
        bool isMaybeFloat() const;
        bool isFloatish() const;
        bool isFloatishDoubleLit() const;
        bool isVoid() const;
        bool isExtern() const;
        bool isVarAsmJsType() const;
        bool isSubType( AsmJsType type ) const;
        bool isSuperType( AsmJsType type ) const;
        const wchar_t *toChars() const;
        bool isSIMDType() const;
        bool isSIMDInt32x4() const;
        bool isSIMDFloat32x4() const;
        bool isSIMDFloat64x2() const;
        AsmJsRetType toRetType() const;
    };

    // Represents the subset of AsmJsType that can be used as the return AsmJsType of a
    // function.
    class AsmJsRetType
    {
    public:
        enum Which
        {
            Void = AsmJsType::Void,
            Signed = AsmJsType::Signed,
            Double = AsmJsType::Double,
            Float = AsmJsType::Float,
            Fixnum = AsmJsType::Fixnum,
            Unsigned = AsmJsType::Unsigned,
            Floatish = AsmJsType::Floatish,
            Int32x4 = AsmJsType::Int32x4,
            Float32x4 = AsmJsType::Float32x4,
            Float64x2 = AsmJsType::Float64x2
        };

    private:
        Which which_;

    public:
        AsmJsRetType();
        AsmJsRetType( Which w );
        AsmJsRetType( AsmJSCoercion coercion );
        Which which() const;
        AsmJsType toType() const;
        AsmJsVarType toVarType() const;

        bool operator==( AsmJsRetType rhs ) const;
        bool operator!=( AsmJsRetType rhs ) const;
    };

    // Represents the subset of Type that can be used as a variable or
    // argument's type. Note: AsmJSCoercion and VarType are kept separate to
    // make very clear the signed/int distinction: a coercion may explicitly sign
    // an *expression* but, when stored as a variable, this signedness information
    // is explicitly thrown away by the asm.js type system. E.g., in
    //
    //   function f(i) {
    //     i = i | 0;             (1)
    //     if (...)
    //         i = foo() >>> 0;
    //     else
    //         i = bar() | 0;
    //     return i | 0;          (2)
    //   }
    //
    // the AsmJSCoercion of (1) is Signed (since | performs ToInt32) but, when
    // translated to an VarType, the result is a plain Int since, as shown, it
    // is legal to assign both Signed and Unsigned (or some other Int) values to
    // it. For (2), the AsmJSCoercion is also Signed but, when translated to an
    // RetType, the result is Signed since callers (asm.js and non-asm.js) can
    // rely on the return value being Signed.
    class AsmJsVarType
    {
    public:
        enum Which : byte
        {
            Int = AsmJsType::Int,
            Double = AsmJsType::Double,
            Float = AsmJsType::Float,
            Int32x4 = AsmJsType::Int32x4,
            Float32x4 = AsmJsType::Float32x4,
            Float64x2 = AsmJsType::Float64x2
        };

    private:
        Which which_;

    public:
        AsmJsVarType();
        AsmJsVarType( Which w );
        AsmJsVarType( AsmJSCoercion coercion );
        Which which() const;
        AsmJsType toType() const;
        AsmJSCoercion toCoercion() const;
        static AsmJsVarType FromCheckedType( AsmJsType type );
        inline bool isInt()const {return which_ == Int; }
        inline bool isDouble()const {return which_ == Double; }
        inline bool isFloat()const {return which_ == Float; }
        inline bool isInt32x4()const    { return which_ == Int32x4; }
        inline bool isFloat32x4()const  { return which_ == Float32x4; }
        inline bool isFloat64x2()const  { return which_ == Float64x2; }
        inline bool isSIMD()    const   { return isInt32x4() || isFloat32x4() || isFloat64x2(); }
        bool operator==( AsmJsVarType rhs ) const;
        bool operator!=( AsmJsVarType rhs ) const;
    };

    // Implements <: (subtype) operator when the RHS is an VarType
    static inline bool
        operator<=( AsmJsType lhs, AsmJsVarType rhs )
    {
        switch( rhs.which() )
        {
        case AsmJsVarType::Int:    return lhs.isInt();
        case AsmJsVarType::Double: return lhs.isDouble();
        case AsmJsVarType::Float:  return lhs.isFloat();
        }
        AssertMsg( false, "Unexpected RHS type" );
    }

    // Base class for all the symbol in Asm.Js during compilation
    // Defined by a type and a name
    class AsmJsSymbol
    {
    public:
        enum SymbolType
        {
            Variable,
            Argument,
            MathConstant,
            ConstantImport,
            ImportFunction,
            FuncPtrTable,
            ModuleFunction,
            ArrayView,
            MathBuiltinFunction,
            TypedArrayBuiltinFunction,
            /*SIMDVariable,*/
            SIMDBuiltinFunction,
            ModuleArgument
        };
    private:
        // name of the symbol, all symbols must have unique names
        PropertyName mName;
        // Type of the symbol, used for casting
        SymbolType   mType;
    public:
        // Constructor
        AsmJsSymbol(PropertyName name, SymbolType type) : mName(name), mType(type) { }

        // Accessor for the name
        inline PropertyName GetName() const{return mName;}
        // Sets the name of the symbol
        inline void SetName(PropertyName name) {mName = name;}
        // Returns the type of the symbol
        inline SymbolType GetSymbolType()const { return mType; }
        // Casts the symbol to a derived class, additional test done to make sure is it the right type
        template<typename T>
        T* Cast();

        // AsmJsSymbol interface
    public:
        // retrieve the type of the symbol when it is use in an expression
        virtual AsmJsType GetType() const = 0;
        // if the symbol is mutable, it can be on the LHS of an assignment operation
        virtual bool isMutable() const = 0;
    };

    // Symbol representing a module argument
    class AsmJsModuleArg : public AsmJsSymbol
    {
    public:
        enum ArgType: int8
        {
            StdLib,
            Import,
            Heap
        };
    private:
        ArgType mArgType;
    public:
        // Constructor
        AsmJsModuleArg(PropertyName name, ArgType type) : AsmJsSymbol(name, AsmJsSymbol::ModuleArgument), mArgType(type) { }
        // Accessor
        inline const ArgType GetArgType()const { return mArgType; }

        // AsmJsSymbol interface
    public:
        virtual AsmJsType GetType() const override;
        virtual bool isMutable() const override;
    };

    // Symbol representing a double constant from the standard library
    class AsmJsMathConst : public AsmJsSymbol
    {
        // address of the constant, lifetime of this address must be for the whole execution of the program (global var)
        const double* mVal;
    public:
        // Constructor
        AsmJsMathConst(PropertyName name, const double* val) : AsmJsSymbol(name, AsmJsSymbol::MathConstant), mVal(val) { }
        // Accessor
        inline const double* GetVal()const { return mVal; }

        // AsmJsSymbol interface
    public:
        virtual AsmJsType GetType() const override;
        virtual bool isMutable() const override;
    };

    // Base class defining Variables in asm.js, can be a variable of the module or a function argument
    class AsmJsVarBase : public AsmJsSymbol
    {
        // type of the variable, isDouble => double registerSpace, isInt => int registerSpace
        AsmJsVarType    mType;
        // register where the value of this variable resides
        RegSlot      mLocation;
        bool         mIsMutable;
    public:
        // Constructor
        AsmJsVarBase(PropertyName name, AsmJsSymbol::SymbolType type, bool isMutable = true) :
            AsmJsSymbol(name, type)
            , mType(AsmJsVarType::Double)
            , mLocation(Js::Constants::NoRegister)
            , mIsMutable(isMutable)
        {
        }

        // Accessors
        inline Js::RegSlot GetLocation() const            { return mLocation; }
        inline void SetLocation( Js::RegSlot val )        { mLocation = val; }
        inline AsmJsVarType GetVarType() const            { return mType; }
        inline void SetVarType( const AsmJsVarType& type ){ mType = type; }

        // AsmJsSymbol interface
    public:
        virtual AsmJsType GetType() const override
        {
            return GetVarType().toType();
        }
        virtual bool isMutable() const override
        {
            return mIsMutable;
        }
    };

    // Defines a Variable, a variable can be changed and has a default value used to initialize the variable.
    // Function and the module can have variables
    class AsmJsVar : public AsmJsVarBase
    {
        // register of the const value that initialize this variable, NoRegister for Args
        union
        {
            double doubleVal;
            float floatVal;
            int intVal;
            AsmJsSIMDValue simdVal;
        }mConstInitialiser;
    public:
        // Constructors
        AsmJsVar( PropertyName name, bool isMutable = true) :
            AsmJsVarBase(name, AsmJsSymbol::Variable, isMutable)
        {
            mConstInitialiser.doubleVal = 0;
        }

        // Accessors
        inline void   SetConstInitialiser ( double val ){ mConstInitialiser.doubleVal = val; }
        inline double GetDoubleInitialiser() const      { return mConstInitialiser.doubleVal; }
        inline void   SetConstInitialiser(float val)   { mConstInitialiser.floatVal = val; }
        inline float    GetFloatInitialiser() const      { return mConstInitialiser.floatVal; }
        inline void   SetConstInitialiser ( int val )   { mConstInitialiser.intVal = val; }
        inline int    GetIntInitialiser   () const      { return mConstInitialiser.intVal; }

        inline void SetConstInitialiser(AsmJsSIMDValue val) { mConstInitialiser.simdVal = val; }
        inline AsmJsSIMDValue GetSimdConstInitialiser()      { return mConstInitialiser.simdVal; }
    };

    // AsmJsArgument defines the arguments of a function
    class AsmJsArgument : public AsmJsVarBase
    {
    public:
        // Constructor
        AsmJsArgument( PropertyName name ) :
            AsmJsVarBase( name, AsmJsSymbol::Argument )
        {
        }
    };

    // AsmJsConstantImport defines a variable that is initialized by an import from the foreign object
    class AsmJsConstantImport : public AsmJsVarBase
    {
        // name of the field used to initialize the variable, i.e.: var i1 = foreign.mField;
        PropertyName mField;

    public:
        // Constructor
        AsmJsConstantImport( PropertyName name, PropertyName field ) :
            AsmJsVarBase( name, AsmJsSymbol::ConstantImport ),
            mField( field )
        {
        }

        // Accessor
        inline Js::PropertyName GetField() const { return mField; }
    };

#if DBG_DUMP
    // Function used to debug Temporary register allocation in the bytecode generator
    template<typename T> void PrintTmpRegisterAllocation( RegSlot loc );
    template<typename T> void PrintTmpRegisterDeAllocation( RegSlot loc );
#endif

    /// Register space for const, parameters, variables and tmp values
    ///     --------------------------------------------------------
    ///     | 0 (Reserved) | Consts  | Parameters | Variables | Tmp
    ///     --------------------------------------------------------
    ///     Cannot allocate in any different order
    template<typename T, RegSlot Reserved_Slots_Count>
    class AsmJsRegisterSpaceGeneric
    {
        // Total number of register allocated
        RegSlot   mRegisterCount;

        // location of the first temporary register and last variable + 1
        RegSlot   mFirstTmpReg;

        // Location of the next register to be allocated
        RegSlot   mNextLocation;

        // number of const, includes the reserved slots
        RegSlot    mNbConst;

    public:
        // Constructor
        AsmJsRegisterSpaceGeneric() :
            mRegisterCount( Reserved_Slots_Count )
            , mFirstTmpReg( Reserved_Slots_Count )
            , mNextLocation( Reserved_Slots_Count )
            , mNbConst( Reserved_Slots_Count )
        {
            CompileAssert( Reserved_Slots_Count >= 0 );
        }
        // Get the number of const allocated
        inline RegSlot GetConstCount() const      { return mNbConst; }
        // Get the location of the first temporary register
        inline RegSlot GetFirstTmpRegister() const{ return mFirstTmpReg; }
        // Get the total number of temporary register allocated
        inline RegSlot GetTmpCount() const        { return mRegisterCount-mFirstTmpReg; }
        // Get number of local variables
        inline RegSlot GetVarCount() const        { return mFirstTmpReg - mNbConst; }
        // Get the total number of variable allocated ( including temporaries )
        inline RegSlot GetTotalVarCount() const        { return mRegisterCount - mNbConst; }
        inline RegSlot GetRegisterCount()const { return mRegisterCount; }

        // Acquire a location for a register. Use only for arguments and Variables
        inline RegSlot AcquireRegister()
        {
            // Makes sure no temporary register have been allocated yet
            Assert( mFirstTmpReg == mRegisterCount && mNextLocation == mFirstTmpReg );
            ++mFirstTmpReg;
            ++mRegisterCount;
            return mNextLocation++;
        }

        // Acquire a location for a constant
        inline RegSlot AcquireConstRegister()
        {
            ++mNbConst;
            return AcquireRegister();
        }

        // Acquire a location for a temporary register
        RegSlot AcquireTmpRegister()
        {
            // Make sure this function is called correctly
            Assert( this->mNextLocation <= this->mRegisterCount && this->mNextLocation >= this->mFirstTmpReg );

            // Allocate a new temp pseudo-register, increasing the locals count if necessary.
            if( this->mNextLocation == this->mRegisterCount )
            {
                ++this->mRegisterCount;
            }
#if DBG_DUMP
            PrintTmpRegisterAllocation<T>( mNextLocation );
#endif
            return mNextLocation++;
        }

        // Release a location for a temporary register, must be the last location acquired
        void ReleaseTmpRegister( RegSlot tmpReg )
        {
            // make sure the location released is valid
            Assert( tmpReg != Constants::NoRegister );

            // Put this reg back on top of the temp stack (if it's a temp).
            if( this->IsTmpReg( tmpReg ) )
            {
                Assert( tmpReg == this->mNextLocation - 1 );
#if DBG_DUMP
                PrintTmpRegisterDeAllocation<T>( mNextLocation-1 );
#endif
                this->mNextLocation--;
            }
        }

        // Checks if the register is a temporary register
        bool IsTmpReg( RegSlot tmpReg )
        {
            Assert( this->mFirstTmpReg != Js::Constants::NoRegister );
            return !IsConstReg( tmpReg ) && tmpReg >= mFirstTmpReg;
        }

        // Checks if the register is a const register
        bool IsConstReg( RegSlot reg )
        {
            // a register is const if it is between the first register and the end of consts
            return reg < mNbConst && reg != 0;
        }

        // Checks if the register is a variable register
        bool IsVarReg( RegSlot reg )
        {
            // a register is a var if it is between the last const and the end
            // equivalent to  reg>=mNbConst && reg<mRegisterCount
            // forcing unsigned, if reg < mNbConst then reg-mNbConst = 0xFFFFF..
            return (uint32_t)( reg - mNbConst ) < (uint32_t)( mRegisterCount - mNbConst );
        }

        // Releases a location if its a temporary, safe to call with any expression
        void ReleaseLocation( const EmitExpressionInfo *pnode )
        {
            // Release the temp assigned to this expression so it can be re-used.
            if( pnode && pnode->location != Js::Constants::NoRegister )
            {
                this->ReleaseTmpRegister( pnode->location );
            }
        }

        // Checks if the location points to a temporary register
        bool IsTmpLocation( const EmitExpressionInfo* pnode )
        {
            if( pnode && pnode->location != Js::Constants::NoRegister )
            {
                return IsTmpReg( pnode->location );
            }
            return false;
        }

        // Checks if the location points to a constant register
        bool IsConstLocation( const EmitExpressionInfo* pnode )
        {
            if( pnode && pnode->location != Js::Constants::NoRegister )
            {
                return IsConstReg( pnode->location );
            }
            return false;
        }

        // Checks if the location points to a variable register
        bool IsVarLocation( const EmitExpressionInfo* pnode )
        {
            if( pnode && pnode->location != Js::Constants::NoRegister )
            {
                return IsVarReg( pnode->location );
            }
            return false;
        }

        // Checks if the location is valid ( within bounds of already allocated registers )
        bool IsValidLocation( const EmitExpressionInfo* pnode )
        {
            if( pnode && pnode->location != Js::Constants::NoRegister )
            {
                return pnode->location < mRegisterCount;
            }
            return false;
        }
    };

    template <typename T>
    struct AsmJsComparer : public DefaultComparer<T> {};

    template <>
    struct AsmJsComparer<float>
    {
        __inline static bool Equals(float x, float y)
        {
            int32 i32x = *(int32*)&x;
            int32 i32y = *(int32*)&y;
            return i32x == i32y;
        }

        __inline static hash_t GetHashCode(float i)
        {
            return (hash_t)i;
        }
    };

    template <>
    struct AsmJsComparer<double>
    {
        __inline static bool Equals(double x, double y)
        {
            int64 i64x = *(int64*)&x;
            int64 i64y = *(int64*)&y;
            return i64x == i64y;
        }

        __inline static hash_t GetHashCode(double d)
        {
            __int64 i64 = *(__int64*)&d;
            return (uint)((i64 >> 32) ^ (uint)i64);
        }
    };

    // Register space use by the function, include a map to quickly find the location assigned to constants
    template<typename T>
    class AsmJsRegisterSpace : public AsmJsRegisterSpaceGeneric < T, 1 > // reserves 1 location for return
    {
        typedef JsUtil::BaseDictionary<T, RegSlot, ArenaAllocator, PowerOf2SizePolicy, AsmJsComparer> ConstMap;
        // Map for constant and their location
        ConstMap mConstMap;
    public:
        // Constructor
        AsmJsRegisterSpace( ArenaAllocator* allocator ) :
            mConstMap( allocator )
        {
        }

        inline void AddConst( T val )
        {
            if( !mConstMap.ContainsKey( val ) )
            {
                mConstMap.Add( val, AcquireConstRegister() );
            }
        }

        inline RegSlot GetConstRegister( T val ) const
        {
            return mConstMap.LookupWithKey( val, Constants::NoRegister );
        }
        inline const ConstMap GetConstMap()
        {
            return mConstMap;
        }
    };

    class AsmJsFunctionDeclaration : public AsmJsSymbol
    {
        AsmJsRetType    mReturnType;
        ArgSlot         mArgCount;
        RegSlot         mLocation;
        AsmJsType*      mArgumentsType;
        bool            mReturnTypeKnown : 1;
    protected:
        ArenaAllocator* mAllocator;
    public:
        AsmJsFunctionDeclaration( PropertyName name, AsmJsSymbol::SymbolType type,  ArenaAllocator* allocator):
            AsmJsSymbol( name, type )
            , mAllocator(allocator)
            , mReturnType( AsmJsRetType::Void )
            , mArgCount(Constants::InvalidArgSlot)
            , mLocation( 0 )
            , mReturnTypeKnown( false )
            , mArgumentsType(nullptr)
        { }
        // returns false if the current return type is known and different
        virtual bool CheckAndSetReturnType( Js::AsmJsRetType val );
        inline Js::AsmJsRetType GetReturnType() const{return mReturnType;}
        bool EnsureArgCount(ArgSlot count);
        void SetArgCount(ArgSlot count );

        ArgSlot GetArgCount() const
        {
            return mArgCount;
        }
        AsmJsType* GetArgTypeArray();

        const AsmJsType& GetArgType( ArgSlot index ) const
        {
            Assert( mArgumentsType && index < GetArgCount() );
            return mArgumentsType[index];
        }
        void SetArgType(const AsmJsType& arg, ArgSlot index)
        {
            Assert( index < GetArgCount() ); mArgumentsType[index] = arg;
        }
        void SetArgType(AsmJsVarBase* arg, ArgSlot index)
        {
            Assert( mArgumentsType != nullptr && index < GetArgCount() );
            SetArgType( arg->GetType(), index );
        }
        bool EnsureArgType(AsmJsVarBase* arg, ArgSlot index);
        inline Js::RegSlot GetFunctionIndex() const{return mLocation;}
        inline void SetFunctionIndex( Js::RegSlot val ){mLocation = val;}

        // argCount : number of arguments to check
        // args : dynamic array with the argument type
        // retType : returnType associated with this function signature
        virtual bool SupportsArgCall(ArgSlot argCount, AsmJsType* args, AsmJsRetType& retType);
        // Return the size in bytes of the arguments, inArgCount is the number of argument in the call ( can be different than mArgCount for FFI )
        ArgSlot GetArgByteSize(ArgSlot inArgCount) const;

        //AsmJsSymbol interface
        virtual AsmJsType GetType() const;
        virtual bool isMutable() const;
    };


    class AsmJsMathFunction : public AsmJsFunctionDeclaration
    {
        AsmJSMathBuiltinFunction mBuiltIn;
        // chain list of supported signature (t1,t2,...) -> retType
        // careful not to create a cycle in the chain
        AsmJsMathFunction* mOverload;
        OpCodeAsmJs mOpCode;
    public:
        AsmJsMathFunction(PropertyName name, ArenaAllocator* allocator, ArgSlot argCount, AsmJSMathBuiltinFunction builtIn, OpCodeAsmJs op, AsmJsRetType retType, ...);

        void SetOverload( AsmJsMathFunction* val );
        AsmJSMathBuiltinFunction GetMathBuiltInFunction(){ return mBuiltIn; };
        virtual bool CheckAndSetReturnType( Js::AsmJsRetType val ) override;
        bool SupportsMathCall(ArgSlot argCount, AsmJsType* args, OpCodeAsmJs& op, AsmJsRetType& retType);
    private:
        virtual bool SupportsArgCall(ArgSlot argCount, AsmJsType* args, AsmJsRetType& retType ) override;

    };

    class AsmJsTypedArrayFunction : public AsmJsFunctionDeclaration
    {
        AsmJSTypedArrayBuiltinFunction mBuiltIn;
        ArrayBufferView::ViewType mType;
    public:
        AsmJsTypedArrayFunction(PropertyName name, ArenaAllocator* allocator, AsmJSTypedArrayBuiltinFunction builtIn, ArrayBufferView::ViewType type) :
            AsmJsFunctionDeclaration(name, AsmJsSymbol::TypedArrayBuiltinFunction, allocator), mBuiltIn(builtIn), mType(type) { }

        AsmJSTypedArrayBuiltinFunction GetArrayBuiltInFunction(){ return mBuiltIn; };
        ArrayBufferView::ViewType GetViewType(){ return mType; };

    };

    class AsmJsImportFunction : public AsmJsFunctionDeclaration
    {
        PropertyName mField;
    public:
        AsmJsImportFunction( PropertyName name, PropertyName field, ArenaAllocator* allocator );

        inline Js::PropertyName GetField() const
        {
            return mField;
        }

        // We cannot know the return type of an Import Function so always think its return type is correct
        virtual bool CheckAndSetReturnType( Js::AsmJsRetType val ) override{return true;}
        virtual bool SupportsArgCall(ArgSlot argCount, AsmJsType* args, AsmJsRetType& retType ) override;
    };

    class AsmJsFunctionTable : public AsmJsFunctionDeclaration
    {
        typedef JsUtil::List<RegSlot, ArenaAllocator> FuncIndexTable;
        FuncIndexTable  mTable;
        unsigned int    mSize;
        bool            mIsDefined : 1;
        bool            mAreArgumentsKnown : 1;
    public:
        AsmJsFunctionTable( PropertyName name, ArenaAllocator* allocator ) :
            AsmJsFunctionDeclaration( name, AsmJsSymbol::FuncPtrTable, allocator )
            , mTable(allocator)
            , mSize( 0 )
            , mIsDefined( false )
            , mAreArgumentsKnown( false )
        {

        }

        inline bool IsDefined() const{return mIsDefined;}
        inline void Define(){mIsDefined = true;}
        inline uint GetSize() const{return mSize;}
        inline void SetSize( unsigned int val )
        {
            mSize = val;
            mTable.EnsureArray( mSize );
        }
        inline void SetModuleFunctionIndex( RegSlot funcIndex, unsigned int index )
        {
            Assert( index < mSize );
            mTable.SetItem( index, funcIndex );
        }
        inline RegSlot GetModuleFunctionIndex( unsigned int index )
        {
            Assert( index < mSize );
            return mTable.Item( index );
        }
        virtual bool SupportsArgCall(ArgSlot argCount, AsmJsType* args, AsmJsRetType& retType );

    };

    class AsmJsFunc : public AsmJsFunctionDeclaration
    {
        typedef JsUtil::BaseDictionary<PropertyId, AsmJsVarBase*, ArenaAllocator> VarNameMap;

        unsigned        mCompileTime; //unused
        VarNameMap      mVarMap;
        ParseNode*      mBodyNode;
        ParseNode*      mFncNode;
        AsmJsRegisterSpace<int> mIntRegisterSpace;
        AsmJsRegisterSpace<float> mFloatRegisterSpace;
        AsmJsRegisterSpace<double> mDoubleRegisterSpace;
        typedef JsUtil::List<AsmJsVarBase*, ArenaAllocator> SIMDVarsList;
        AsmJsRegisterSpace<AsmJsSIMDValue> mSimdRegisterSpace;
        SIMDVarsList                 mSimdVarsList;

        FuncInfo*       mFuncInfo;
        FunctionBody*   mFuncBody;
        int             mArgOutDepth;
        int             mMaxArgOutDepth;
        ULONG           mOrigParseFlags;
        bool            mDeferred;
        bool            mDefined : 1; // true when compiled completely without any errors
    public:
        AsmJsFunc( PropertyName name, ParseNode* pnodeFnc, ArenaAllocator* allocator );

        unsigned GetCompileTime() const { return mCompileTime; }
        void AccumulateCompileTime(unsigned ms) { mCompileTime += ms; }

        inline ParseNode* GetFncNode() const{ return mFncNode; }
        inline void       SetFncNode(ParseNode* fncNode) { mFncNode = fncNode; }
        inline FuncInfo*  GetFuncInfo() const{ return mFuncInfo; }
        inline void       SetFuncInfo(FuncInfo* fncInfo) { mFuncInfo = fncInfo; }
        inline FunctionBody*GetFuncBody() const{ return mFuncBody; }
        inline void       SetFuncBody(FunctionBody* fncBody) { mFuncBody = fncBody; }
        inline ULONG      GetOrigParseFlags() const{ return mOrigParseFlags; }
        inline void       SetOrigParseFlags(ULONG parseFlags) { mOrigParseFlags = parseFlags; }

        inline ParseNode* GetBodyNode() const{return mBodyNode;}
        inline void SetBodyNode( ParseNode* val ){mBodyNode = val;}
        inline void Finish() { mDefined = true; }
        inline bool IsDefined()const { return mDefined; }
        inline void SetDeferred() { mDeferred = true; }
        inline bool IsDeferred()const { return mDeferred; }
        template<typename T> inline AsmJsRegisterSpace<T>& GetRegisterSpace() {return *(AsmJsRegisterSpace<T>*)&mIntRegisterSpace;}
        template<> inline AsmJsRegisterSpace<int>& GetRegisterSpace(){return mIntRegisterSpace;}
        template<> inline AsmJsRegisterSpace<double>& GetRegisterSpace(){return mDoubleRegisterSpace;}
        template<> inline AsmJsRegisterSpace<float>& GetRegisterSpace(){ return mFloatRegisterSpace; }

        template<> inline AsmJsRegisterSpace<AsmJsSIMDValue>& GetRegisterSpace() { return mSimdRegisterSpace; }
        inline SIMDVarsList& GetSimdVarsList()    { return mSimdVarsList;  }

        /// Wrapper for RegisterSpace methods
        template<typename T> inline RegSlot AcquireRegister   (){return GetRegisterSpace<T>().AcquireRegister();}
        template<typename T> inline void AddConst             ( T val ){GetRegisterSpace<T>().AddConst( val );}
        template<typename T> inline RegSlot GetConstRegister  ( T val ){return GetRegisterSpace<T>().GetConstRegister( val );}
        template<typename T> inline RegSlot AcquireTmpRegister(){return GetRegisterSpace<T>().AcquireTmpRegister();}
        template<typename T> inline void ReleaseTmpRegister   ( Js::RegSlot tmpReg ){GetRegisterSpace<T>().ReleaseTmpRegister( tmpReg );}
        template<typename T> inline void ReleaseLocation      ( const EmitExpressionInfo* pnode ){GetRegisterSpace<T>().ReleaseLocation( pnode );}
        template<typename T> inline bool IsTmpLocation        ( const EmitExpressionInfo* pnode ){return GetRegisterSpace<T>().IsTmpLocation( pnode );}
        template<typename T> inline bool IsConstLocation      ( const EmitExpressionInfo* pnode ){return GetRegisterSpace<T>().IsConstLocation( pnode );}
        template<typename T> inline bool IsVarLocation        ( const EmitExpressionInfo* pnode ){return GetRegisterSpace<T>().IsVarLocation( pnode );}
        template<typename T> inline bool IsValidLocation      ( const EmitExpressionInfo* pnode ){return GetRegisterSpace<T>().IsValidLocation( pnode );}
        void ReleaseLocationGeneric( const EmitExpressionInfo* pnode );

        // Search for a var in the varMap of the function, return nullptr if not found
        AsmJsVarBase* FindVar( const PropertyName name ) const;
        // Defines a new variable int the function, return nullptr if already exists or theres an error
        AsmJsVarBase* DefineVar(PropertyName name, bool isArg = false, bool isMutable = true);
        AsmJsSymbol* LookupIdentifier( const PropertyName name, AsmJsLookupSource::Source* lookupSource = nullptr ) const;
        void SetArgOutDepth(int outParamsCount);
        void UpdateMaxArgOutDepth(int outParamsCount);
        inline int GetArgOutDepth() const{ return mArgOutDepth; }
        inline int GetMaxArgOutDepth() const{ return mMaxArgOutDepth; }

    };

    struct MathBuiltin
    {
        enum Kind
        {
            Function, Constant
        };
        Kind kind;
        AsmJSMathBuiltinFunction mathLibFunctionName;
        union
        {
            const double* cst;
            AsmJsMathFunction* func;
        } u;

        MathBuiltin() : kind( Kind( -1 ) )
        {
        }
        MathBuiltin(AsmJSMathBuiltinFunction mathLibFunctionName, const double* cst) : kind(Constant), mathLibFunctionName(mathLibFunctionName)
        {
            u.cst = cst;
        }
        MathBuiltin(AsmJSMathBuiltinFunction mathLibFunctionName, AsmJsMathFunction* func) : kind(Function), mathLibFunctionName(mathLibFunctionName)
        {
            u.func = func;
        }
    };

    struct TypedArrayBuiltin
    {
        AsmJSTypedArrayBuiltinFunction mArrayLibFunctionName;
        AsmJsTypedArrayFunction* mFunc;

        TypedArrayBuiltin() { }
        TypedArrayBuiltin(AsmJSTypedArrayBuiltinFunction arrayLibFunctionName, AsmJsTypedArrayFunction* func) :
            mArrayLibFunctionName(arrayLibFunctionName),
            mFunc(func)
        { }
    };

    class AsmJsArrayView : public AsmJsSymbol
    {
        ArrayBufferView::ViewType mViewType;

    public:
        AsmJsArrayView( PropertyName name, ArrayBufferView::ViewType viewType ) :
            AsmJsSymbol( name, AsmJsSymbol::ArrayView )
            , mViewType( viewType )
        {

        }

        virtual AsmJsType GetType() const;
        virtual bool isMutable() const;
        inline ArrayBufferView::ViewType GetViewType() const
        {
            return mViewType;
        }
    };

    class AsmJsFunctionInfo
    {
        int mIntConstCount, mDoubleConstCount, mFloatConstCount;
        ArgSlot mArgCount;
        int mIntVarCount, mDoubleVarCount, mFloatVarCount, mIntTmpCount, mDoubleTmpCount, mFloatTmpCount;
        AsmJsVarType::Which * mArgType;
        ArgSlot mArgSizesLength;
        uint * mArgSizes;
        ArgSlot mArgByteSize;
        // offset in Byte from the beggining of the stack aka R0
        int mIntByteOffset, mDoubleByteOffset, mFloatByteOffset;
        AsmJsRetType mReturnType;

        bool mIsHeapBufferConst;
        bool mUsesHeapBuffer;
        int mSimdConstCount, mSimdVarCount, mSimdTmpCount, mSimdByteOffset;

        FunctionBody* asmJsModuleFunctionBody;
    public:
        AsmJsFunctionInfo() : mArgCount(0),
                              mIntConstCount(0),
                              mFloatConstCount(0),
                              mDoubleConstCount(0),
                              mIntVarCount(0),
                              mDoubleVarCount(0),
                              mFloatVarCount(0),
                              mIntTmpCount(0),
                              mDoubleTmpCount(0),
                              mFloatTmpCount(0),
                              mArgSizesLength(0),
                              mIntByteOffset(0),
                              mDoubleByteOffset(0),
                              mFloatByteOffset(0),
                              mReturnType(AsmJsRetType::Void),
                              mArgByteSize(0),
                              mSimdConstCount(0),
                              mSimdVarCount(0),
                              mSimdTmpCount(0),
                              mSimdByteOffset(0),
                              asmJsModuleFunctionBody(nullptr),
                              mTJBeginAddress(nullptr),
                              mUsesHeapBuffer(false),
                              mIsHeapBufferConst(false),
                              mArgType(nullptr),
                              mArgSizes(nullptr) {}
        // the key is the bytecode address
        typedef JsUtil::BaseDictionary<int, ptrdiff_t, Recycler> ByteCodeToTJMap;
        ByteCodeToTJMap* mbyteCodeTJMap;
        BYTE* mTJBeginAddress;
        inline int GetDoubleConstCount() const{ return mDoubleConstCount; }
        inline void SetDoubleConstCount(int val) { mDoubleConstCount = val; }
        inline int GetFloatConstCount() const{ return mFloatConstCount; }
        inline void SetFloatConstCount(int val) { mFloatConstCount = val; }
        inline int GetIntConstCount() const{return mIntConstCount;}
        inline void SetIntConstCount(int val) { mIntConstCount = val; }
        inline int GetIntVarCount()const { return mIntVarCount; }
        inline void SetIntVarCount(int val) { mIntVarCount = val; }
        inline int GetFloatVarCount()const { return mFloatVarCount; }
        inline void SetFloatVarCount(int val) { mFloatVarCount = val; }
        inline int GetDoubleVarCount()const { return mDoubleVarCount; }
        inline void SetDoubleVarCount(int val) { mDoubleVarCount = val; }
        inline int GetIntTmpCount()const { return mIntTmpCount; }
        inline void SetIntTmpCount(int val) { mIntTmpCount = val; }
        inline int GetFloatTmpCount()const { return mFloatTmpCount; }
        inline void SetFloatTmpCount(int val) { mFloatTmpCount = val; }
        inline int GetDoubleTmpCount()const { return mDoubleTmpCount; }
        inline void SetDoubleTmpCount(int val) { mDoubleTmpCount = val; }
        inline ArgSlot GetArgCount() const{ return mArgCount; }
        inline void SetArgCount(ArgSlot val) { mArgCount = val; }
        inline AsmJsRetType GetReturnType() const{return mReturnType;}
        inline void SetReturnType(AsmJsRetType val) { mReturnType = val; }
        inline ArgSlot GetArgByteSize() const{return mArgByteSize;}
        inline void SetArgByteSize(ArgSlot val) { mArgByteSize = val; }
        inline int GetDoubleByteOffset() const{ return mDoubleByteOffset; }
        inline void SetDoubleByteOffset(int val) { mDoubleByteOffset = val; }
        inline int GetFloatByteOffset() const{ return mFloatByteOffset; }
        inline void SetFloatByteOffset(int val) { mFloatByteOffset = val; }
        inline int GetIntByteOffset() const{ return mIntByteOffset; }
        inline void SetIntByteOffset(int val) { mIntByteOffset = val; }

        inline void SetIsHeapBufferConst(bool val) { mIsHeapBufferConst = val; }
        inline bool IsHeapBufferConst() const{ return mIsHeapBufferConst; }

        inline void SetUsesHeapBuffer(bool val) { mUsesHeapBuffer = val; }
        inline bool UsesHeapBuffer() const{ return mUsesHeapBuffer; }

        inline int GetSimdConstCount() const { return mSimdConstCount;  }
        inline void SetSimdConstCount(int val) { mSimdConstCount = val; }
        inline int GetSimdVarCount() const { return mSimdVarCount; }
        inline void SetSimdVarCount(int val) { mSimdVarCount = val; }
        inline int GetSimdTmpCount() const { return mSimdTmpCount; }
        inline void SetSimdTmpCount(int val) { mSimdTmpCount = val; }
        inline int GetSimdByteOffset() const { return mSimdByteOffset; }
        inline void SetSimdByteOffset(int val) { mSimdByteOffset = val; }
        inline int GetSimdAllCount() const { return GetSimdConstCount() + GetSimdVarCount() + GetSimdTmpCount(); }

        int GetTotalSizeinBytes()const;
        void SetArgType(AsmJsVarType type, ArgSlot index);
        inline AsmJsVarType GetArgType(ArgSlot index ) const
        {
            Assert(mArgCount != Constants::InvalidArgSlot);
            AnalysisAssert( index < mArgCount);
            return mArgType[index];
        }
        bool Init( AsmJsFunc* func );
        void SetModuleFunctionBody(FunctionBody* body){ asmJsModuleFunctionBody = body; };
        FunctionBody* GetModuleFunctionBody()const{ return asmJsModuleFunctionBody; };

        ArgSlot GetArgSizeArrayLength()
        {
            return mArgSizesLength;
        }
        void SetArgSizeArrayLength(ArgSlot val)
        {
            mArgSizesLength = val;
        }

        uint* GetArgsSizesArray()
        {
            return mArgSizes;
        }
        void SetArgsSizesArray(uint* val)
        {
            mArgSizes = val;
        }
        AsmJsVarType::Which * GetArgTypeArray()
        {
            return mArgType;
        }
        void SetArgTypeArray(AsmJsVarType::Which* val)
        {
            mArgType = val;
        }


    };

    // The asm.js spec recognizes this set of builtin SIMD functions.
    // !! Note: keep these grouped by SIMD type
    enum AsmJsSIMDBuiltinFunction
    {
#define ASMJS_SIMD_NAMES(name, propertyName) AsmJsSIMDBuiltin_##name,
#include "AsmJsBuiltinNames.h"
        AsmJsSIMDBuiltin_COUNT
    };

    // SIMD built-in function symbol
    // Do we have overloads for any SIMD function ?
    class AsmJsSIMDFunction : public AsmJsFunctionDeclaration
    {
        AsmJsSIMDBuiltinFunction mBuiltIn;
        AsmJsSIMDFunction* mOverload;
        OpCodeAsmJs mOpCode;
    public:
        AsmJsSIMDFunction(PropertyName name, ArenaAllocator* allocator, ArgSlot argCount, AsmJsSIMDBuiltinFunction builtIn, OpCodeAsmJs op, AsmJsRetType retType, ...);

        PropertyId GetBuiltinPropertyId();
        void SetOverload(AsmJsSIMDFunction* val);
        AsmJsSIMDBuiltinFunction GetSimdBuiltInFunction(){ return mBuiltIn; };
        virtual bool CheckAndSetReturnType(Js::AsmJsRetType val) override;

        bool SupportsSIMDCall(ArgSlot argCount, AsmJsType* args, OpCodeAsmJs& op, AsmJsRetType& retType);

        bool IsConstructor();
        bool IsConstructor(uint argCount);
        bool IsTypeCheck();  // e.g. float32x4(x)
        bool IsInt32x4Func() { return mBuiltIn >= AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_Int32x4 && mBuiltIn < AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_Float32x4; }
        bool IsFloat32x4Func() { return mBuiltIn >= AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_Float32x4 && mBuiltIn < AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_Float64x2; }
        bool IsFloat64x2Func() { return mBuiltIn >= AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_Float64x2 && mBuiltIn < AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_COUNT; }

        bool IsSimdLoadFunc()
        {
            return (mBuiltIn >= AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_int32x4_load && mBuiltIn <= AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_int32x4_load3) ||
                (mBuiltIn >= AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_float32x4_load && mBuiltIn <= AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_float32x4_load3) ||
                (mBuiltIn >= AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_float64x2_load && mBuiltIn <= AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_float64x2_load1);
        }
        bool IsSimdStoreFunc()
        {
            return (mBuiltIn >= AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_int32x4_store && mBuiltIn <= AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_int32x4_store3) ||
                (mBuiltIn >= AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_float32x4_store && mBuiltIn <= AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_float32x4_store3) ||
                (mBuiltIn >= AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_float64x2_store && mBuiltIn <= AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_float64x2_store1);
        }
        bool IsExtractLaneFunc()
        {
            return (
                mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_int32x4_extractLane ||
                mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_float32x4_extractLane
                );
        }
        bool IsReplaceLaneFunc()
        {
            return (
                mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_int32x4_replaceLane ||
                mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_float32x4_replaceLane
                );
        }
        bool IsLaneAccessFunc()
        {
            return (
                IsExtractLaneFunc() || IsReplaceLaneFunc()
                );
        }

        bool IsShuffleFunc()
        {
            return  (
                mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_int32x4_shuffle ||
                mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_float32x4_shuffle ||
                mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_float64x2_shuffle
                );
        }

        bool IsSwizzleFunc()
        {
            return  (
                mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_int32x4_swizzle ||
                mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_float32x4_swizzle ||
                mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_float64x2_swizzle
                );
        }

        static bool SameTypeOperations(AsmJsSIMDFunction *func1, AsmJsSIMDFunction *func2)
        {
            bool result = func1->IsFloat32x4Func() && func2->IsFloat32x4Func();
            result = result || (func1->IsFloat64x2Func() && func2->IsFloat64x2Func());
            result = result || (func1->IsInt32x4Func() && func2->IsInt32x4Func());
            return result;
        }

        AsmJsVarType GetTypeCheckVarType();
        AsmJsVarType GetConstructorVarType();

        OpCodeAsmJs GetOpcode() { return mOpCode;  }

    private:
        virtual bool SupportsArgCall(ArgSlot argCount, AsmJsType* args, AsmJsRetType& retType) override;
    };

};
#endif
