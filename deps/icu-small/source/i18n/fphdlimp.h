// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2009-2015, International Business Machines Corporation and    *
* others. All Rights Reserved.                                                *
*******************************************************************************
*/

#ifndef FPHDLIMP_H
#define FPHDLIMP_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/fieldpos.h"
#include "unicode/fpositer.h"

U_NAMESPACE_BEGIN

// utility FieldPositionHandler
// base class, null implementation

class U_I18N_API FieldPositionHandler: public UMemory {
 protected:
  int32_t fShift = 0;

 public:
  virtual ~FieldPositionHandler();
  virtual void addAttribute(int32_t id, int32_t start, int32_t limit) = 0;
  virtual void shiftLast(int32_t delta) = 0;
  virtual UBool isRecording(void) const = 0;

  void setShift(int32_t delta);
};


// utility subclass FieldPositionOnlyHandler

class FieldPositionOnlyHandler : public FieldPositionHandler {
  FieldPosition& pos;

 public:
  FieldPositionOnlyHandler(FieldPosition& pos);
  virtual ~FieldPositionOnlyHandler();

  void addAttribute(int32_t id, int32_t start, int32_t limit) U_OVERRIDE;
  void shiftLast(int32_t delta) U_OVERRIDE;
  UBool isRecording(void) const U_OVERRIDE;
};


// utility subclass FieldPositionIteratorHandler

class FieldPositionIteratorHandler : public FieldPositionHandler {
  FieldPositionIterator* iter; // can be NULL
  UVector32* vec;
  UErrorCode status;

  // Note, we keep a reference to status, so if status is on the stack, we have
  // to be destroyed before status goes out of scope.  Easiest thing is to
  // allocate us on the stack in the same (or narrower) scope as status has.
  // This attempts to encourage that by blocking heap allocation.
  void *operator new(size_t s);
  void *operator new[](size_t s);

 public:
  FieldPositionIteratorHandler(FieldPositionIterator* posIter, UErrorCode& status);
  ~FieldPositionIteratorHandler();

  void addAttribute(int32_t id, int32_t start, int32_t limit) U_OVERRIDE;
  void shiftLast(int32_t delta) U_OVERRIDE;
  UBool isRecording(void) const U_OVERRIDE;
};

U_NAMESPACE_END

#endif /* !UCONFIG_NO_FORMATTING */

#endif /* FPHDLIMP_H */
