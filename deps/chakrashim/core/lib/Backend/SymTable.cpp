//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "BackEnd.h"


void
SymTable::Init(Func* func)
{
    m_func = func;
    m_propertyMap = JitAnew(func->m_alloc, PropertyMap, func->m_alloc);
    m_propertyEquivBvMap = JitAnew(func->m_alloc, PropertyEquivBvMap, func->m_alloc);
}

///----------------------------------------------------------------------------
///
/// SymTable::Add
///
///     Add newSym to this symbol table.
///
///----------------------------------------------------------------------------

void
SymTable::Add(Sym * newSym)
{
    int hash;

    newSym->m_id += this->m_IDAdjustment;

    hash = this->Hash(newSym->m_id);

    AssertMsg(newSym->m_next == NULL, "Error inserting a symbol in the SymTable with a non-NULL next ptr.");

    newSym->m_next = m_table[hash];
    m_table[hash] = newSym;

    if (newSym->IsPropertySym())
    {
        PropertySym * propertySym = newSym->AsPropertySym();
        if (propertySym->m_fieldKind != PropertyKindWriteGuard)
        {
            SymIdPropIdPair pair(propertySym->m_stackSym->m_id, propertySym->m_propertyId);
#if DBG
            PropertySym * foundPropertySym;
            Assert(!this->m_propertyMap->TryGetValue(pair, &foundPropertySym));
#endif
            this->m_propertyMap->Add(pair, propertySym);
        }

        if (propertySym->m_fieldKind == PropertyKindSlots ||
            propertySym->m_fieldKind == PropertyKindData ||
            propertySym->m_fieldKind == PropertyKindWriteGuard)
        {
            BVSparse<JitArenaAllocator> *bvEquivSet;

            if (!this->m_propertyEquivBvMap->TryGetValue(propertySym->m_propertyId, &bvEquivSet))
            {
                bvEquivSet = JitAnew(this->m_func->m_alloc, BVSparse<JitArenaAllocator>, this->m_func->m_alloc);
                this->m_propertyEquivBvMap->Add(propertySym->m_propertyId, bvEquivSet);
            }
            bvEquivSet->Set(propertySym->m_id);
            propertySym->m_propertyEquivSet = bvEquivSet;
        }
    }

    m_func->OnAddSym(newSym);
}

///----------------------------------------------------------------------------
///
/// SymTable::Find
///
///     Find the symbol with the given SymID in this table.
///     Returns NULL if it can't find one.
///
///----------------------------------------------------------------------------

Sym *
SymTable::Find(SymID id) const
{
    int hash;

    id += this->m_IDAdjustment;

    hash = this->Hash(id);

    for (Sym *sym = m_table[hash]; sym != NULL; sym = sym->m_next)
    {
        if (sym->m_id == id)
        {
            return sym;
        }
    }

    return NULL;
}

///----------------------------------------------------------------------------
///
/// SymTable::FindStackSym
///
///     Find the stack symbol with the given SymID in this table.  If the
///     symbol exists, it needs to be a StackSym.
///     Returns NULL if it can't find one.
///
///----------------------------------------------------------------------------

StackSym *
SymTable::FindStackSym(SymID id) const
{
    Sym *   sym;

    sym = this->Find(id);

    if (sym)
    {
        AssertMsg(sym->IsStackSym(), "Looking for StackSym, found something else");
        return sym->AsStackSym();
    }

    return NULL;
}

///----------------------------------------------------------------------------
///
/// SymTable::FindPropertySym
///
///     Find the field symbol with the given SymID in this table.  If the
///     symbol exists, it needs to be a PropertySym.
///     Returns NULL if it can't find one.
///
///----------------------------------------------------------------------------

PropertySym *
SymTable::FindPropertySym(SymID id) const
{
    Sym *   sym;

    sym = this->Find(id);

    if (sym)
    {
        AssertMsg(sym->IsPropertySym(), "Looking for PropertySym, found something else");
        return sym->AsPropertySym();
    }

    return NULL;
}

///----------------------------------------------------------------------------
///
/// SymTable::FindPropertySym
///
///     Find the stack symbol with the corresponding to given StackSym and
///     propertyId.
///     Returns NULL if it can't find one.
///
///----------------------------------------------------------------------------

PropertySym *
SymTable::FindPropertySym(SymID stackSymID, int32 propertyId) const
{
    PropertySym *  propertySym;

    stackSymID += this->m_IDAdjustment;

    SymIdPropIdPair pair(stackSymID, propertyId);
    if (this->m_propertyMap->TryGetValue(pair, &propertySym))
    {
        Assert(propertySym->m_propertyId == propertyId);
        Assert(propertySym->m_stackSym->m_id == stackSymID);

        return propertySym;
    }
    return NULL;
}

///----------------------------------------------------------------------------
///
/// SymTable::Hash
///
///----------------------------------------------------------------------------

int
SymTable::Hash(SymID id) const
{
    return (id % k_symTableSize);
}

///----------------------------------------------------------------------------
///
/// SymTable::SetStartingID
///
///----------------------------------------------------------------------------

void
SymTable::SetStartingID(SymID startingID)
{
    AssertMsg(m_currentID == 0, "SymTable::SetStarting() can only be called before any symbols are allocated");

    m_currentID = startingID;
}

void
SymTable::IncreaseStartingID(SymID IDIncrease)
{
    m_currentID += IDIncrease;
}

///----------------------------------------------------------------------------
///
/// SymTable::NewID
///
///----------------------------------------------------------------------------

SymID
SymTable::NewID()
{
    SymID newId = m_currentID++;

    AssertMsg(m_currentID > newId, "Too many symbols: m_currentID overflow!");

    return newId - m_IDAdjustment;
}

///----------------------------------------------------------------------------
///
/// SymTable::GetArgSlotSym
///
///     Get a StackSym to represent this argument slot.
///
///----------------------------------------------------------------------------

StackSym *
SymTable::GetArgSlotSym(Js::ArgSlot argSlotNum)
{
    StackSym * argSym;

    argSym = StackSym::NewArgSlotSym(argSlotNum, m_func);
    argSym->m_offset = (argSlotNum - 1) * MachPtr;
    argSym->m_allocated = true;
    return argSym;
}

///----------------------------------------------------------------------------
///
/// SymTable::GetMaxSymID
///
///     Returns the largest SymID in the table at this point.
///
///----------------------------------------------------------------------------

SymID
SymTable::GetMaxSymID() const
{
    return m_currentID - 1;
}

///----------------------------------------------------------------------------
///
/// SymTable::ClearStackSymScratch
///
///----------------------------------------------------------------------------

void
SymTable::ClearStackSymScratch()
{
    FOREACH_SYM_IN_TABLE(sym, this)
    {
        if (sym->IsStackSym())
        {
            memset(&(sym->AsStackSym()->scratch), 0, sizeof(sym->AsStackSym()->scratch));
        }
    } NEXT_SYM_IN_TABLE;
}
