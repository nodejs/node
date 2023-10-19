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
#include "unicode/formattedvalue.h"

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
  virtual UBool isRecording() const = 0;

  void setShift(int32_t delta);
};


// utility subclass FieldPositionOnlyHandler

class FieldPositionOnlyHandler : public FieldPositionHandler {
  FieldPosition& pos;
  UBool acceptFirstOnly = false;
  UBool seenFirst = false;

 public:
  FieldPositionOnlyHandler(FieldPosition& pos);
  virtual ~FieldPositionOnlyHandler();

  void addAttribute(int32_t id, int32_t start, int32_t limit) override;
  void shiftLast(int32_t delta) override;
  UBool isRecording() const override;

  /**
   * Enable this option to lock in the FieldPosition value after seeing the
   * first occurrence of the field. The default behavior is to take the last
   * occurrence.
   */
  void setAcceptFirstOnly(UBool acceptFirstOnly);
};


// utility subclass FieldPositionIteratorHandler
// exported as U_I18N_API for tests

class U_I18N_API FieldPositionIteratorHandler : public FieldPositionHandler {
  FieldPositionIterator* iter; // can be nullptr
  UVector32* vec;
  UErrorCode status;
  UFieldCategory fCategory;

  // Note, we keep a reference to status, so if status is on the stack, we have
  // to be destroyed before status goes out of scope.  Easiest thing is to
  // allocate us on the stack in the same (or narrower) scope as status has.
  // This attempts to encourage that by blocking heap allocation.
  static void* U_EXPORT2 operator new(size_t) noexcept = delete;
  static void* U_EXPORT2 operator new[](size_t) noexcept = delete;
#if U_HAVE_PLACEMENT_NEW
  static void* U_EXPORT2 operator new(size_t, void*) noexcept = delete;
#endif

 public:
  FieldPositionIteratorHandler(FieldPositionIterator* posIter, UErrorCode& status);
  /** If using this constructor, you must call getError() when done formatting! */
  FieldPositionIteratorHandler(UVector32* vec, UErrorCode& status);
  ~FieldPositionIteratorHandler();

  void addAttribute(int32_t id, int32_t start, int32_t limit) override;
  void shiftLast(int32_t delta) override;
  UBool isRecording() const override;

  /** Copies a failed error code into _status. */
  inline void getError(UErrorCode& _status) {
    if (U_SUCCESS(_status) && U_FAILURE(status)) {
      _status = status;
    }
  }

  inline void setCategory(UFieldCategory category) {
    fCategory = category;
  }
};

U_NAMESPACE_END

#endif /* !UCONFIG_NO_FORMATTING */

#endif /* FPHDLIMP_H */
