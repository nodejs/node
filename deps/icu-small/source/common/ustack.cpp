/*
**********************************************************************
*   Copyright (C) 2003-2011, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*/

#include "uvector.h"

U_NAMESPACE_BEGIN

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(UStack)

UStack::UStack(UErrorCode &status) :
    UVector(status)
{
}

UStack::UStack(int32_t initialCapacity, UErrorCode &status) :
    UVector(initialCapacity, status)
{
}

UStack::UStack(UObjectDeleter *d, UElementsAreEqual *c, UErrorCode &status) :
    UVector(d, c, status)
{
}

UStack::UStack(UObjectDeleter *d, UElementsAreEqual *c, int32_t initialCapacity, UErrorCode &status) :
    UVector(d, c, initialCapacity, status)
{
}

UStack::~UStack() {}

void* UStack::pop(void) {
    int32_t n = size() - 1;
    void* result = 0;
    if (n >= 0) {
        result = elementAt(n);
        removeElementAt(n);
    }
    return result;
}

int32_t UStack::popi(void) {
    int32_t n = size() - 1;
    int32_t result = 0;
    if (n >= 0) {
        result = elementAti(n);
        removeElementAt(n);
    }
    return result;
}

int32_t UStack::search(void* obj) const {
    int32_t i = indexOf(obj);
    return (i >= 0) ? size() - i : i;
}

U_NAMESPACE_END
