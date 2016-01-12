//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once


///----------------------------------------------------------------------------
///
/// Sym::IsStackSym
///
///----------------------------------------------------------------------------

inline bool
Sym::IsStackSym() const
{
    return m_kind == SymKindStack;
}

///----------------------------------------------------------------------------
///
/// Sym::AsStackSym
///
///     Use this symbol as a StackSym.
///
///----------------------------------------------------------------------------

inline StackSym *
Sym::AsStackSym()
{
    AssertMsg(this->IsStackSym(), "Bad call to AsStackSym()");

    return reinterpret_cast<StackSym *>(this);
}

///----------------------------------------------------------------------------
///
/// Sym::IsStackSym
///
///----------------------------------------------------------------------------

inline bool
Sym::IsPropertySym() const
{
    return m_kind == SymKindProperty;
}

///----------------------------------------------------------------------------
///
/// Sym::AsPropertySym
///
///     Use this symbol as a PropertySym.
///
///----------------------------------------------------------------------------

inline PropertySym *
Sym::AsPropertySym()
{
    AssertMsg(this->IsPropertySym(), "Bad call to AsPropertySym()");

    return reinterpret_cast<PropertySym *>(this);
}

///----------------------------------------------------------------------------
///
/// Sym::IsArgSlotSym
///
///----------------------------------------------------------------------------

inline bool
StackSym::IsArgSlotSym() const
{
    if(m_isArgSlotSym)
    {
        Assert(this->m_argSlotNum != 0);
    }
    return m_isArgSlotSym;
}

///----------------------------------------------------------------------------
///
/// Sym::IsParamSlotSym
///
///----------------------------------------------------------------------------

inline bool
StackSym::IsParamSlotSym() const
{
    return m_paramSlotNum != InvalidSlot;
}

///----------------------------------------------------------------------------
///
/// Sym::IsAllocated
///
///----------------------------------------------------------------------------

inline bool
StackSym::IsAllocated() const
{
    return m_allocated;
}
