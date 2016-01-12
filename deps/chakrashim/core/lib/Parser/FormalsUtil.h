//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once
template <class Fn, bool mapRest>
void MapFormalsImpl(ParseNode *pnodeFunc, Fn fn)
{
    for (ParseNode *pnode = pnodeFunc->sxFnc.pnodeArgs; pnode != nullptr; pnode = pnode->GetFormalNext())
    {
        fn(pnode);
    }
    if (mapRest && pnodeFunc->sxFnc.pnodeRest != nullptr)
    {
        fn(pnodeFunc->sxFnc.pnodeRest);
    }
}

template <class Fn>
void MapFormalsWithoutRest(ParseNode *pnodeFunc, Fn fn)
{
    return MapFormalsImpl<Fn, false>(pnodeFunc, fn);
}

template <class Fn>
void MapFormals(ParseNode *pnodeFunc, Fn fn)
{
    return MapFormalsImpl<Fn, true>(pnodeFunc, fn);
}

template <class Fn>
void MapFormalsFromPattern(ParseNode *pnodeFunc, Fn fn)
{
    for (ParseNode *pnode = pnodeFunc->sxFnc.pnodeArgs; pnode != nullptr; pnode = pnode->GetFormalNext())
    {
        if (pnode->nop == knopParamPattern)
        {
            Parser::MapBindIdentifier(pnode->sxParamPattern.pnode1, fn);
        }
    }
}

