//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace JsUtil
{
    template<class T>
    class DoublyLinkedListElement
    {
    private:
        T *previous, *next;

    public:
        DoublyLinkedListElement();

    public:
        T *Previous() const;
        T *Next() const;

        template<class D> static bool Contains(D *const element, D *const head);
        template<class D> static bool ContainsSubsequence(D *const first, D *const last, D *const head);

        template<class D> static void LinkToBeginning(D *const element, D * *const head, D * *const tail);
        template<class D> static void LinkToEnd(D *const element, D * *const head, D * *const tail);
        template<class D> static void LinkBefore(D *const element, D *const nextElement, D * *const head, D * *const tail);
        template<class D> static void LinkAfter(D *const element, D *const previousElement, D * *const head, D * *const tail);
        template<class D> static void UnlinkFromBeginning(D *const element, D * *const head, D * *const tail);
        template<class D> static void UnlinkFromEnd(D *const element, D * *const head, D * *const tail);
        template<class D> static void UnlinkPartial(D *const element, D * *const head, D * *const tail);
        template<class D> static void Unlink(D *const element, D * *const head, D * *const tail);
        template<class D> static void MoveToBeginning(D *const element, D * *const head, D * *const tail);

        template<class D> static void UnlinkSubsequenceFromEnd(D *const first, D * *const head, D * *const tail);
        template<class D> static void UnlinkSubsequence(D *const first, D *const last, D * *const head, D * *const tail);
        template<class D> static void MoveSubsequenceToBeginning(D *const first, D *const last, D * *const head, D * *const tail);

        // JScriptDiag doesn't seem to like the PREVENT_COPY macro
    private:
        DoublyLinkedListElement(const DoublyLinkedListElement &other);
        DoublyLinkedListElement &operator =(const DoublyLinkedListElement &other);
    };
}
