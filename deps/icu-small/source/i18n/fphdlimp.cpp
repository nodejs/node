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

void
FieldPositionHandler::addAttribute(int32_t, int32_t, int32_t) {
}

void
FieldPositionHandler::shiftLast(int32_t) {
}

UBool
FieldPositionHandler::isRecording(void) const {
  return FALSE;
}


// utility subclass FieldPositionOnlyHandler

FieldPositionOnlyHandler::FieldPositionOnlyHandler(FieldPosition& _pos)
  : pos(_pos) {
}

FieldPositionOnlyHandler::~FieldPositionOnlyHandler() {
}

void
FieldPositionOnlyHandler::addAttribute(int32_t id, int32_t start, int32_t limit) {
  if (pos.getField() == id) {
    pos.setBeginIndex(start);
    pos.setEndIndex(limit);
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
FieldPositionOnlyHandler::isRecording(void) const {
  return pos.getField() != FieldPosition::DONT_CARE;
}


// utility subclass FieldPositionIteratorHandler

FieldPositionIteratorHandler::FieldPositionIteratorHandler(FieldPositionIterator* posIter,
                                                           UErrorCode& _status)
    : iter(posIter), vec(NULL), status(_status) {
  if (iter && U_SUCCESS(status)) {
    vec = new UVector32(status);
  }
}

FieldPositionIteratorHandler::~FieldPositionIteratorHandler() {
  // setData adopts the vec regardless of status, so it's safe to null it
  if (iter) {
    iter->setData(vec, status);
  }
  // if iter is null, we never allocated vec, so no need to free it
  vec = NULL;
}

void
FieldPositionIteratorHandler::addAttribute(int32_t id, int32_t start, int32_t limit) {
  if (iter && U_SUCCESS(status) && start < limit) {
    int32_t size = vec->size();
    vec->addElement(id, status);
    vec->addElement(start, status);
    vec->addElement(limit, status);
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
FieldPositionIteratorHandler::isRecording(void) const {
  return U_SUCCESS(status);
}

U_NAMESPACE_END

#endif /* !UCONFIG_NO_FORMATTING */
