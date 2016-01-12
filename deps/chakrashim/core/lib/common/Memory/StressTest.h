//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#if DBG

// Recycler requires 16/32 byte alignment
const int OBJALIGN = sizeof(void*) * 4;

enum CreateOptions
{
    NormalObj,
    LeafObj
};

template<class T> T AlignPtr(T ptr, size_t align)
{
    return reinterpret_cast<T>(::Math::Align(reinterpret_cast<size_t>(ptr), align));
}

struct TestObject
{
    // Full object size
    size_t size;

    // Number of pointers to other objects this object potentially has
    int pointerCount;

    // Hash of part of the object's contents, used for corruption detection
    size_t cookie;

    TestObject(size_t _size, int _pointerCount);
    size_t CalculateCookie();
    void CheckCookie();

    // Sets an object pointer at index indicated
    void Set(int index, TestObject *val);
    void SetRandom(TestObject *val);
    void CreateFalseReferenceRandom(TestObject *val);

    // Does a best-effort attempt to add an object pointer to an unused slot
    void Add(TestObject *val);

    // Clears the first non-null pointer in the list
    void ClearOne();

    // Retrieves a pointer
    TestObject* Get(int index);

    TestObject** GetDataPointer() { return reinterpret_cast<TestObject**>(AlignPtr((char*)this + sizeof(TestObject), OBJALIGN)); }

    static void Visit(Recycler *recycler, TestObject *root);
    template<class Fn> static void Visit(Recycler *recycler, TestObject *root, Fn fn);
    static TestObject* Create(Recycler *recycler, int pointerCount, size_t extraBytes, CreateOptions options = NormalObj);
};

class StressTester
{
    Recycler *recycler;

    static const int MaxLinkedListLength = 100;
    static const int MaxTreeDepth = 8;
    static const int MaxNodesInTree = 1000;

    size_t GetRandomSize();
    TestObject *CreateRandom();
    TestObject* CreateLinkedList();
    TestObject* CreateTree();
    int treeTotal;
    void CreateTreeHelper(TestObject *root, int depth);

public:
    StressTester(Recycler *_recycler);
    void Run();
};

#endif
