//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace IR {

///----------------------------------------------------------------------------
///
/// Opnd::Use
///
///     If this operand is not inUse, use it.  Otherwise, make a copy.
///
///----------------------------------------------------------------------------

inline Opnd *
Opnd::Use(Func *func)
{
    if (!m_inUse)
    {
        m_inUse = true;
        return this;
    }

    Opnd * newOpnd = this->Copy(func);
    newOpnd->m_inUse = true;

    return newOpnd;
}

///----------------------------------------------------------------------------
///
/// Opnd::UnUse
///
///----------------------------------------------------------------------------

inline void
Opnd::UnUse()
{
    AssertMsg(m_inUse, "Expected inUse to be set...");
    m_inUse = false;
}

///----------------------------------------------------------------------------
///
/// Opnd::IsSymOpnd
///
///----------------------------------------------------------------------------

inline bool
Opnd::IsSymOpnd() const
{
    return GetKind() == OpndKindSym;
}

///----------------------------------------------------------------------------
///
/// Opnd::AsSymOpnd
///
///     Use this opnd as a SymOpnd.
///
///----------------------------------------------------------------------------

inline SymOpnd *
Opnd::AsSymOpnd()
{
    AssertMsg(this->IsSymOpnd(), "Bad call to AsSymOpnd()");

    return reinterpret_cast<SymOpnd *>(this);
}

inline PropertySymOpnd *
Opnd::AsPropertySymOpnd()
{
    AssertMsg(this->IsSymOpnd() && this->AsSymOpnd()->IsPropertySymOpnd(), "Bad call to AsPropertySymOpnd()");

    return reinterpret_cast<PropertySymOpnd *>(this);
}

///----------------------------------------------------------------------------
///
/// Opnd::IsRegOpnd
///
///----------------------------------------------------------------------------

inline bool
Opnd::IsRegOpnd() const
{
    return GetKind() == OpndKindReg;
}

///----------------------------------------------------------------------------
///
/// Opnd::AsRegOpnd
///
///     Use this opnd as a RegOpnd.
///
///----------------------------------------------------------------------------

inline const RegOpnd *
Opnd::AsRegOpnd() const
{
    AssertMsg(this->IsRegOpnd(), "Bad call to AsRegOpnd()");

    return reinterpret_cast<const RegOpnd *>(this);
}

///----------------------------------------------------------------------------
///
/// Opnd::AsRegOpnd
///
///     Use this opnd as a RegOpnd.
///
///----------------------------------------------------------------------------

inline RegOpnd *
Opnd::AsRegOpnd()
{
    AssertMsg(this->IsRegOpnd(), "Bad call to AsRegOpnd()");

    return reinterpret_cast<RegOpnd *>(this);
}

///----------------------------------------------------------------------------
///
/// Opnd::IsRegBVOpnd
///
///----------------------------------------------------------------------------

inline bool
Opnd::IsRegBVOpnd() const
{
    return GetKind() == OpndKindRegBV;
}

///----------------------------------------------------------------------------
///
/// Opnd::AsRegBVOpnd
///
///     Use this opnd as a RegBVOpnd.
///
///----------------------------------------------------------------------------
inline RegBVOpnd *
Opnd::AsRegBVOpnd()
{
    AssertMsg(this->IsRegBVOpnd(), "Bad call to AsRegOpnd()");

    return reinterpret_cast<RegBVOpnd *>(this);
}

///----------------------------------------------------------------------------
///
/// Opnd::IsIntConstOpnd
///
///----------------------------------------------------------------------------

inline bool
Opnd::IsIntConstOpnd() const
{
    return GetKind() == OpndKindIntConst;
}

///----------------------------------------------------------------------------
///
/// Opnd::AsIntConstOpnd
///
///     Use this opnd as a IntConstOpnd.
///
///----------------------------------------------------------------------------

inline IntConstOpnd *
Opnd::AsIntConstOpnd()
{
    AssertMsg(this->IsIntConstOpnd(), "Bad call to AsIntConstOpnd()");

    return reinterpret_cast<IntConstOpnd *>(this);
}

///----------------------------------------------------------------------------
///
/// Opnd::IsFloatConstOpnd
///
///----------------------------------------------------------------------------

inline bool
Opnd::IsFloatConstOpnd() const
{
    return GetKind() == OpndKindFloatConst;
}

///----------------------------------------------------------------------------
///
/// Opnd::AsFloatConstOpnd
///
///     Use this opnd as a FloatConstOpnd.
///
///----------------------------------------------------------------------------

inline FloatConstOpnd *
Opnd::AsFloatConstOpnd()
{
    AssertMsg(this->IsFloatConstOpnd(), "Bad call to AsFloatConstOpnd()");

    return reinterpret_cast<FloatConstOpnd *>(this);
}

inline bool
Opnd::IsSimd128ConstOpnd() const
{
    return GetKind() == OpndKindSimd128Const;
}

inline Simd128ConstOpnd *
Opnd::AsSimd128ConstOpnd()
{
    AssertMsg(this->IsSimd128ConstOpnd(), "Bad call to AsSimd128ConstOpnd()");

    return reinterpret_cast<Simd128ConstOpnd *>(this);
}

///----------------------------------------------------------------------------
///
/// Opnd::IsHelperCallOpnd
///
///----------------------------------------------------------------------------

inline bool
Opnd::IsHelperCallOpnd() const
{
    return GetKind() == OpndKindHelperCall;
}

///----------------------------------------------------------------------------
///
/// Opnd::AsHelperCallOpnd
///
///     Use this opnd as a HelperCallOpnd.
///
///----------------------------------------------------------------------------

inline HelperCallOpnd *
Opnd::AsHelperCallOpnd()
{
    AssertMsg(this->IsHelperCallOpnd(), "Bad call to AsHelperCallOpnd()");

    return reinterpret_cast<HelperCallOpnd *>(this);
}

///----------------------------------------------------------------------------
///
/// Opnd::IsAddrOpnd
///
///----------------------------------------------------------------------------

inline bool
Opnd::IsAddrOpnd() const
{
    return GetKind() == OpndKindAddr;
}

///----------------------------------------------------------------------------
///
/// Opnd::AsAddrOpnd
///
///     Use this opnd as a AddrOpnd.
///
///----------------------------------------------------------------------------

inline AddrOpnd *
Opnd::AsAddrOpnd()
{
    AssertMsg(this->IsAddrOpnd(), "Bad call to AsAddrOpnd()");

    return reinterpret_cast<AddrOpnd *>(this);
}

///----------------------------------------------------------------------------
///
/// Opnd::IsIndirOpnd
///
///----------------------------------------------------------------------------

inline bool
Opnd::IsIndirOpnd() const
{
    return GetKind() == OpndKindIndir;
}

///----------------------------------------------------------------------------
///
/// Opnd::AsIndirOpnd
///
///     Use this opnd as a IndirOpnd.
///
///----------------------------------------------------------------------------

inline IndirOpnd *
Opnd::AsIndirOpnd()
{
    AssertMsg(this->IsIndirOpnd(), "Bad call to AsIndirOpnd()");

    return reinterpret_cast<IndirOpnd *>(this);
}

///----------------------------------------------------------------------------
///
/// Opnd::IsMemRefOpnd
///
///----------------------------------------------------------------------------

inline bool
Opnd::IsMemRefOpnd() const
{
    return GetKind() == OpndKindMemRef;
}

///----------------------------------------------------------------------------
///
/// Opnd::AsMemRefOpnd
///
///     Use this opnd as a MemRefOpnd.
///
///----------------------------------------------------------------------------

inline MemRefOpnd *
Opnd::AsMemRefOpnd()
{
    AssertMsg(this->IsMemRefOpnd(), "Bad call to AsMemRefOpnd()");

    return reinterpret_cast<MemRefOpnd *>(this);
}

inline bool
Opnd::IsLabelOpnd() const
{
    return GetKind() == OpndKindLabel;
}

inline LabelOpnd *
Opnd::AsLabelOpnd()
{
    AssertMsg(this->IsLabelOpnd(), "Bad call to AsLabelOpnd()");

    return reinterpret_cast<LabelOpnd *>(this);
}

///----------------------------------------------------------------------------
///
/// Opnd::IsImmediateOpnd
///
///----------------------------------------------------------------------------

inline bool
Opnd::IsImmediateOpnd() const
{
    return this->IsIntConstOpnd() || this->IsAddrOpnd() || this->IsHelperCallOpnd();
}


inline bool Opnd::IsMemoryOpnd() const
{
    switch(GetKind())
    {
        case OpndKindSym:
        case OpndKindIndir:
        case OpndKindMemRef:
            return true;
    }
    return false;
}

///----------------------------------------------------------------------------
///
/// Opnd::IsConstOpnd
///
///----------------------------------------------------------------------------

inline bool
Opnd::IsConstOpnd() const
{
    bool result =  this->IsImmediateOpnd() || this->IsFloatConstOpnd();

    result = result || this->IsSimd128ConstOpnd();
    return result;
}

///----------------------------------------------------------------------------
///
/// RegOpnd::AsArrayRegOpnd
///
///----------------------------------------------------------------------------

inline ArrayRegOpnd *RegOpnd::AsArrayRegOpnd()
{
    Assert(IsArrayRegOpnd());
    return static_cast<ArrayRegOpnd *>(this);
}

///----------------------------------------------------------------------------
///
/// RegOpnd::GetReg
///
///----------------------------------------------------------------------------

inline RegNum
RegOpnd::GetReg() const
{
    return m_reg;
}

///----------------------------------------------------------------------------
///
/// RegOpnd::SetReg
///
///----------------------------------------------------------------------------

inline void
RegOpnd::SetReg(RegNum reg)
{
    m_reg = reg;
}

///----------------------------------------------------------------------------
///
/// IndirOpnd::GetBaseOpnd
///
///----------------------------------------------------------------------------

inline RegOpnd *
IndirOpnd::GetBaseOpnd() const
{
    return this->m_baseOpnd;
}

///----------------------------------------------------------------------------
///
/// IndirOpnd::GetIndexOpnd
///
///----------------------------------------------------------------------------

inline RegOpnd *
IndirOpnd::GetIndexOpnd()
{
    return m_indexOpnd;
}

///----------------------------------------------------------------------------
///
/// IndirOpnd::GetOffset
///
///----------------------------------------------------------------------------

inline int32
IndirOpnd::GetOffset() const
{
    return m_offset;
}

///----------------------------------------------------------------------------
///
/// IndirOpnd::SetOffset
///
///----------------------------------------------------------------------------

inline void
IndirOpnd::SetOffset(int32 offset, bool dontEncode /* = false */)
{
    m_offset = offset;
    m_dontEncode = dontEncode;
}

///----------------------------------------------------------------------------
///
/// IndirOpnd::GetScale
///
///----------------------------------------------------------------------------

inline byte
IndirOpnd::GetScale() const
{
    return m_scale;
}

///----------------------------------------------------------------------------
///
/// IndirOpnd::SetScale
///
///----------------------------------------------------------------------------

inline void
IndirOpnd::SetScale(byte scale)
{
    m_scale = scale;
}

///----------------------------------------------------------------------------
///
/// MemRefOpnd::GetMemLoc
///
///----------------------------------------------------------------------------

inline void *
MemRefOpnd::GetMemLoc() const
{
    return m_memLoc;
}

///----------------------------------------------------------------------------
///
/// MemRefOpnd::SetMemLoc
///
///----------------------------------------------------------------------------

inline void
MemRefOpnd::SetMemLoc(void * pMemLoc)
{
    m_memLoc = pMemLoc;
}

inline LabelInstr *
LabelOpnd::GetLabel() const
{
    return m_label;
}

inline void
LabelOpnd::SetLabel(LabelInstr * labelInstr)
{
    m_label = labelInstr;
}

inline BVUnit32
RegBVOpnd::GetValue() const
{
    return m_value;
}

} // namespace IR
