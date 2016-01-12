//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace JsUtil
{
    template<class T>
    class DoublyLinkedList
    {
    private:
        T *head, *tail;

    public:
        DoublyLinkedList();

    public:
        T *Head() const;
        T *Tail() const;

    public:
        bool Contains(T *const element) const;
        bool ContainsSubsequence(T *const first, T *const last) const;

    public:
        bool IsEmpty();
        void Clear();
        void LinkToBeginning(T *const element);
        void LinkToEnd(T *const element);
        void LinkBefore(T *const element, T *const nextElement);
        void LinkAfter(T *const element, T *const previousElement);
        T *UnlinkFromBeginning();
        T *UnlinkFromEnd();
        void UnlinkPartial(T *const element);
        void Unlink(T *const element);
        void MoveToBeginning(T *const element);
        void UnlinkSubsequenceFromEnd(T *const first);
        void UnlinkSubsequence(T *const first, T *const last);
        void MoveSubsequenceToBeginning(T *const first, T *const last);

        // JScriptDiag doesn't seem to like the PREVENT_COPY macro
    private:
        DoublyLinkedList(const DoublyLinkedList &other);
        DoublyLinkedList &operator =(const DoublyLinkedList &other);
    };
}
