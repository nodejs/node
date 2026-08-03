// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2009-2015, International Business Machines Corporation and
* others. All Rights Reserved.
*******************************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "fphdlimp.h"
#include "uvectr32.h"

U_NAMESPACE_BEGIN

// utility FieldPositionHandler
// base class, null implementation

FieldPositionHandler::~FieldPositionHandler() {
}

void FieldPositionHandler::setShift(int32_t delta) {
  fShift = delta;
}


// utility subclass FieldPositionOnlyHandler

FieldPositionOnlyHandler::FieldPositionOnlyHandler(FieldPosition& _pos)
  : pos(_pos) {
}

FieldPositionOnlyHandler::~FieldPositionOnlyHandler() {
}

void
FieldPositionOnlyHandler::addAttribute(int32_t id, int32_t start, int32_t limit) {
  if (pos.getField() == id && (!acceptFirstOnly || !seenFirst)) {
    seenFirst = true;
    pos.setBeginIndex(start + fShift);
    pos.setEndIndex(limit + fShift);
  }
}

void
FieldPositionOnlyHandler::shiftLast(int32_t delta) {
  if (delta != 0 && pos.getField() != FieldPosition::DONT_CARE && pos.getBeginIndex() != -1) {
    pos.setBeginIndex(delta + pos.getBeginIndex());
    pos.setEndIndex(delta + pos.getEndIndex());
  }
}

UBool
FieldPositionOnlyHandler::isRecording() const {
  return pos.getField() != FieldPosition::DONT_CARE;
}

void FieldPositionOnlyHandler::setAcceptFirstOnly(UBool acceptFirstOnly) {
  this->acceptFirstOnly = acceptFirstOnly;
}


// utility subclass FieldPositionIteratorHandler

FieldPositionIteratorHandler::FieldPositionIteratorHandler(FieldPositionIterator* posIter,
                                                           UErrorCode& _status)
    : iter(posIter), vec(nullptr), status(_status), fCategory(UFIELD_CATEGORY_UNDEFINED) {
  if (iter && U_SUCCESS(status)) {
    vec = new UVector32(status);
  }
}

FieldPositionIteratorHandler::FieldPositionIteratorHandler(
    UVector32* vec,
    UErrorCode& status)
    : iter(nullptr), vec(vec), status(status), fCategory(UFIELD_CATEGORY_UNDEFINED) {
}

FieldPositionIteratorHandler::~FieldPositionIteratorHandler() {
  // setData adopts the vec regardless of status, so it's safe to null it
  if (iter) {
    iter->setData(vec, status);
  }
  // if iter is null, we never allocated vec, so no need to free it
  vec = nullptr;
}

void
FieldPositionIteratorHandler::addAttribute(int32_t id, int32_t start, int32_t limit) {
  if (vec && U_SUCCESS(status) && start < limit) {
    int32_t size = vec->size();
    vec->addElement(fCategory, status);
    vec->addElement(id, status);
    vec->addElement(start + fShift, status);
    vec->addElement(limit + fShift, status);
    if (!U_SUCCESS(status)) {
      vec->setSize(size);
    }
  }
}

void
FieldPositionIteratorHandler::shiftLast(int32_t delta) {
  if (U_SUCCESS(status) && delta != 0) {
    int32_t i = vec->size();
    if (i > 0) {
      --i;
      vec->setElementAt(delta + vec->elementAti(i), i);
      --i;
      vec->setElementAt(delta + vec->elementAti(i), i);
    }
  }
}

UBool
FieldPositionIteratorHandler::isRecording() const {
  return U_SUCCESS(status);
}

U_NAMESPACE_END

#endif /* !UCONFIG_NO_FORMATTING */
