/*
**********************************************************************
*   Copyright (C) 1999-2011, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*/

#include "unicode/chariter.h"

U_NAMESPACE_BEGIN

ForwardCharacterIterator::~ForwardCharacterIterator() {}
ForwardCharacterIterator::ForwardCharacterIterator()
: UObject()
{}
ForwardCharacterIterator::ForwardCharacterIterator(const ForwardCharacterIterator &other)
: UObject(other)
{}


CharacterIterator::CharacterIterator()
: textLength(0), pos(0), begin(0), end(0) {
}

CharacterIterator::CharacterIterator(int32_t length)
: textLength(length), pos(0), begin(0), end(length) {
    if(textLength < 0) {
        textLength = end = 0;
    }
}

CharacterIterator::CharacterIterator(int32_t length, int32_t position)
: textLength(length), pos(position), begin(0), end(length) {
    if(textLength < 0) {
        textLength = end = 0;
    }
    if(pos < 0) {
        pos = 0;
    } else if(pos > end) {
        pos = end;
    }
}

CharacterIterator::CharacterIterator(int32_t length, int32_t textBegin, int32_t textEnd, int32_t position)
: textLength(length), pos(position), begin(textBegin), end(textEnd) {
    if(textLength < 0) {
        textLength = 0;
    }
    if(begin < 0) {
        begin = 0;
    } else if(begin > textLength) {
        begin = textLength;
    }
    if(end < begin) {
        end = begin;
    } else if(end > textLength) {
        end = textLength;
    }
    if(pos < begin) {
        pos = begin;
    } else if(pos > end) {
        pos = end;
    }
}

CharacterIterator::~CharacterIterator() {}

CharacterIterator::CharacterIterator(const CharacterIterator &that) :
ForwardCharacterIterator(that),
textLength(that.textLength), pos(that.pos), begin(that.begin), end(that.end)
{
}

CharacterIterator &
CharacterIterator::operator=(const CharacterIterator &that) {
    ForwardCharacterIterator::operator=(that);
    textLength = that.textLength;
    pos = that.pos;
    begin = that.begin;
    end = that.end;
    return *this;
}

// implementing first[32]PostInc() directly in a subclass should be faster
// but these implementations make subclassing a little easier
UChar
CharacterIterator::firstPostInc(void) {
    setToStart();
    return nextPostInc();
}

UChar32
CharacterIterator::first32PostInc(void) {
    setToStart();
    return next32PostInc();
}

U_NAMESPACE_END
