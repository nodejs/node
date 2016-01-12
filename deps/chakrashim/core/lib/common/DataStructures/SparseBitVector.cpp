//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonDataStructuresPch.h"

BVSparseNode::BVSparseNode(BVIndex beginIndex, BVSparseNode * nextNode) :
    startIndex(beginIndex),
    data(0),
    next(nextNode)
{
}
void BVSparseNode::init(BVIndex beginIndex, BVSparseNode * nextNode)
{
    this->startIndex = beginIndex;
    this->data = 0;
    this->next = nextNode;
}

bool BVSparseNode::ToString(
    __out_ecount(strSize) char *const str,
    const size_t strSize,
    size_t *const writtenLengthRef,
    const bool isInSequence,
    const bool isFirstInSequence,
    const bool isLastInSequence) const
{
    Assert(str);
    Assert(!isFirstInSequence || isInSequence);
    Assert(!isLastInSequence || isInSequence);

    if (strSize == 0)
    {
        if (writtenLengthRef)
        {
            *writtenLengthRef = 0;
        }
        return false;
    }
    str[0] = '\0';

    const size_t reservedLength = _countof(", ...}");
    if (strSize <= reservedLength)
    {
        if (writtenLengthRef)
        {
            *writtenLengthRef = 0;
        }
        return false;
    }

    size_t length = 0;
    if (!isInSequence || isFirstInSequence)
    {
        str[length++] = '{';
    }

    bool insertComma = isInSequence && !isFirstInSequence;
    char tempStr[13];
    for (BVIndex i = data.GetNextBit(); i != BVInvalidIndex; i = data.GetNextBit(i + 1))
    {
        const size_t copyLength = sprintf_s(tempStr, insertComma ? ", %u" : "%u", startIndex + i);
        Assert(static_cast<int>(copyLength) > 0);

        Assert(strSize > length);
        Assert(strSize - length > reservedLength);
        if (strSize - length - reservedLength <= copyLength)
        {
            strcpy_s(&str[length], strSize - length, insertComma ? ", ...}" : "...}");
            if (writtenLengthRef)
            {
                *writtenLengthRef = length + (insertComma ? _countof(", ...}") : _countof("...}"));
            }
            return false;
        }

        strcpy_s(&str[length], strSize - length - reservedLength, tempStr);
        length += copyLength;
        insertComma = true;
    }
    if (!isInSequence || isLastInSequence)
    {
        Assert(_countof("}") < strSize - length);
        strcpy_s(&str[length], strSize - length, "}");
        length += _countof("}");
    }
    if (writtenLengthRef)
    {
        *writtenLengthRef = length;
    }
    return true;
}
