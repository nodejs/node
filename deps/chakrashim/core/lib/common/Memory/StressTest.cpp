//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonMemoryPch.h"

#if DBG
#include "common\Int32Math.h"
#include "DataStructures\list.h"
#include "Memory\StressTest.h"
#include <malloc.h>

typedef JsUtil::BaseDictionary<TestObject*, bool, RecyclerNonLeafAllocator> ObjectTracker_t;
typedef JsUtil::List<TestObject*, Recycler> ObjectList_t;

template<size_t align> bool IsAligned(void *p)
{
    return (reinterpret_cast<size_t>(p) & (align - 1)) == 0;
}

TestObject::TestObject(size_t _size, int _pointerCount) : size(_size), pointerCount(_pointerCount)
{
    cookie = CalculateCookie();
    memset(GetDataPointer(), 0, pointerCount * sizeof(TestObject*));
}

size_t TestObject::CalculateCookie()
{
    return reinterpret_cast<size_t>(this) ^ (static_cast<size_t>(pointerCount) << 12) ^ (size << 24) + 1;
}

void TestObject::CheckCookie()
{

    Assert((reinterpret_cast<size_t>(this)& (OBJALIGN - 1)) == 0);

    Assert(cookie == CalculateCookie());
}

TestObject *TestObject::Get(int index)
{
    Assert(index < pointerCount);

    TestObject *addr = GetDataPointer()[index];

    Assert((reinterpret_cast<size_t>(addr) & (OBJALIGN - 1)) == 0);

    return addr;
}

void TestObject::Set(int index, TestObject *val)
{
    Assert(index < pointerCount);

    GetDataPointer()[index] = val;
}

void TestObject::SetRandom(TestObject *val)
{
    if (pointerCount != 0)
    {
        Set(rand() % pointerCount, val);
    }
}

void TestObject::Add(TestObject *val)
{
    TestObject **data = GetDataPointer();

    for (int i = 0; i < pointerCount; ++i)
    {
        if (data[i] == nullptr/* || !IsAligned<64>(data[i])*/)
        {
            data[i] = val;
            break;
        }
    }
}

void TestObject::ClearOne()
{
    CheckCookie();

    TestObject **data = GetDataPointer();

    for (int i = 0; i < pointerCount; ++i)
    {
        if (data[i] != nullptr/* && IsAligned<64>(data[i])*/)
        {
            // CreateFalseReferenceRandom(data[i]);
            data[i] = nullptr;
            break;
        }
    }
}

void TestObject::Visit(Recycler *recycler, TestObject *root)
{
    Visit(recycler, root, [](TestObject*) { });
}

template<class Fn> void TestObject::Visit(Recycler *recycler, TestObject *root, Fn fn)
{
    // TODO: move these allocations to HeapAllocator.

    ObjectTracker_t *objectTracker = RecyclerNew(recycler, ObjectTracker_t, recycler);
    ObjectList_t *objectList = RecyclerNew(recycler, ObjectList_t, recycler);

    // Prime the list with the first object
    objectList->Add(root);
    objectTracker->Add(root, true);

    int numObjects = 0;
    while (objectList->Count() > 0)
    {
        TestObject *curr = objectList->Item(0);
        objectList->RemoveAt(0);

        curr->CheckCookie();
        for (int i = 0; i < curr->pointerCount; ++i)
        {
            TestObject *obj = curr->Get(i);
            if (obj != nullptr /*&& IsAligned<64>(obj)*/ && !objectTracker->ContainsKey(obj))
            {
                objectTracker->Add(obj, true);
                objectList->Add(obj);
            }
        }

        ++numObjects;
    }

    objectTracker->Map([&](TestObject * val, bool) {
        fn(val);
    });

}

TestObject* TestObject::Create(Recycler *recycler, int pointerCount, size_t extraBytes, CreateOptions options)
{
    size_t size = sizeof(TestObject)+pointerCount * sizeof(TestObject*) + extraBytes;

    if (options == NormalObj)
    {
        return RecyclerNewPlus(recycler, size, TestObject, size, pointerCount);
    }
    else if (options == LeafObj)
    {
        Assert(pointerCount == 0);
        return RecyclerNewPlusLeaf(recycler, size, TestObject, size, pointerCount);
    }
    else
    {
        Assert(false);
        return nullptr;
    }
}

void TestObject::CreateFalseReferenceRandom(TestObject *val)
{
    char *addr = reinterpret_cast<char*>(val);
    addr += 32;
    SetRandom(reinterpret_cast<TestObject*>(addr));
}

StressTester::StressTester(Recycler *_recycler) : recycler(_recycler)
{
    uint seed = (uint)time(NULL);
    printf("Random seed: %u\n", seed);
    srand(seed);
}

size_t StressTester::GetRandomSize()
{
    int i = rand() % 5;
    switch (i)
    {
    case 0: return 0;
    case 1: return rand() % 16;
    case 2: return rand() % 4096;
    case 3: return rand() % 16384;
    case 4: return rand();
    default:
        Assert(false);
        return 0;
    }
}

TestObject* StressTester::CreateLinkedList()
{
    TestObject *root = TestObject::Create(recycler, 1, GetRandomSize());
    TestObject *curr = root;
    int length = rand() % MaxLinkedListLength;
    for (int i = 0; i < length; ++i)
    {
        CreateOptions options = (i == length - 1) ? LeafObj : NormalObj;
        TestObject *next = TestObject::Create(recycler, options == LeafObj ? 0 : 1, GetRandomSize());
        curr->Add(next);
        curr = next;
    }
    return root;
}


void StressTester::CreateTreeHelper(TestObject *root, int depth) {
    for (int i = 0; i < root->pointerCount; ++i, ++treeTotal)
    {
        if (depth == 0 || treeTotal > MaxNodesInTree)
        {
            root->Add(TestObject::Create(recycler, 0, rand(), LeafObj));
        }
        else
        {
            TestObject *newObj = TestObject::Create(recycler, 4, GetRandomSize());
            CreateTreeHelper(newObj, depth - 1);
            root->Add(newObj);
        }
    }
};

TestObject* StressTester::CreateTree()
{
    TestObject *root = TestObject::Create(recycler, 4, GetRandomSize());
    treeTotal = 0;
    CreateTreeHelper(root, rand() % MaxTreeDepth);
    return root;
}

TestObject *StressTester::CreateRandom()
{
    int numObjects = rand() % 5000 + 1;

    void *memory = _alloca(numObjects * sizeof(TestObject*)+OBJALIGN);
    TestObject **objs = reinterpret_cast<TestObject**>(AlignPtr(memory, OBJALIGN));

    // Create the objects
    for (int i = 0; i < numObjects; ++i)
    {
        objs[i] = TestObject::Create(recycler, 10, rand());
    }

    // Create links between objects
    for (int i = 0; i < numObjects; ++i)
    {
        for (int j = 0; j < 5; ++j)
        {
            objs[i]->SetRandom(objs[rand() % numObjects]);
        }
    }

    return objs[0];
}

void StressTester::Run()
{

    const int stackExtraBytes = 1000;
    const int stackPointers = 50;
    const size_t sizeRequired = sizeof(TestObject)+stackExtraBytes + stackPointers * sizeof(TestObject*) + OBJALIGN;
    char memory[sizeRequired];
    void *addr = AlignPtr(memory, OBJALIGN);

    TestObject *stack = new (addr) TestObject(stackExtraBytes, stackPointers);

    auto ObjectVisitor = [&](TestObject *object) {
        // Clear out one of the pointers.
        if (rand() % 5 == 0)
        {
            object->ClearOne();
        }

        // Maybe store a pointer on the stack.
        if (rand() % 25 == 0)
        {
            stack->SetRandom(object);
        }

        // Maybe add a stack reference to the current object
        if (rand() % 25 == 0)
        {
            object->SetRandom(stack->Get(rand() % stack->pointerCount));
        }

    };

    while (1)
    {
        TestObject *root = CreateLinkedList();
        TestObject::Visit(recycler, root);

        root = CreateTree();
        TestObject::Visit(recycler, root, ObjectVisitor);
        TestObject::Visit(recycler, root);

        root = CreateRandom();
        TestObject::Visit(recycler, root, ObjectVisitor);
        TestObject::Visit(recycler, root);

        TestObject::Visit(recycler, stack, [&](TestObject *object) {
            if (rand() % 10 == 0)
            {
                object->ClearOne();
            }
        });

        if (rand() % 3 == 0)
        {
            for (int i = 0; i < stack->pointerCount; ++i)
            {
                stack->Set(i, nullptr);
            }
        }
    }
}
#endif
