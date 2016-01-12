//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
template <class T>
struct LargeStackBlock {
    T* items;
    int index;
    int itemCount;

    static LargeStackBlock<T>* Make(ArenaAllocator* alloc,int itemCount) {
        LargeStackBlock<T>* block = AnewStruct(alloc, LargeStackBlock<T>);
        block->itemCount=itemCount;
        block->items = AnewArray(alloc, T, itemCount);
        block->index=0;
        return block;
    }

    BOOL Full() { return index>=itemCount; }
    BOOL Empty() { return index==0; }

    void Push(T item) {
        AssertMsg(!Full(),"can't push to full stack block");
        items[index++]=item;
    }

    T Pop() {
        AssertMsg(!Empty(),"can't pop empty stack block");
        index--;
        return items[index];
    }
};



template <class T>
class LargeStack {
    SList<LargeStackBlock<T>*>* blockStack;
    static const int BlockSize=8;
    static const int GrowSize=128;
    ArenaAllocator* alloc;

    LargeStack(ArenaAllocator* alloc) : alloc(alloc) {
        blockStack=Anew(alloc,SList<LargeStackBlock<T>*>,alloc);
        blockStack->Push(LargeStackBlock<T>::Make(alloc,BlockSize));
    }
public:
    static LargeStack * New(ArenaAllocator* alloc)
    {
        return Anew(alloc, LargeStack, alloc);
    }

    void Push(T item) {
        LargeStackBlock<T>* top=blockStack->Top();
        if (top->Full()) {
            top=LargeStackBlock<T>::Make(alloc,top->itemCount+GrowSize);
            blockStack->Push(top);
        }
        top->Push(item);
    }

    BOOL Empty() {
        LargeStackBlock<T>* top=blockStack->Top();
        if (top->Empty()) {
            if (blockStack->HasOne()) {
                // Avoid popping the last empty block to reduce freelist overhead.
                return true;
            }

            blockStack->Pop();
            return blockStack->Empty();
        }
        else return false;
    }

    T Pop() {
        LargeStackBlock<T>* top=blockStack->Top();
        if (top->Empty()) {
            blockStack->Pop();
            AssertMsg(!blockStack->Empty(),"can't pop empty block stack");
            top=blockStack->Top();
        }
        return top->Pop();
    }
};
