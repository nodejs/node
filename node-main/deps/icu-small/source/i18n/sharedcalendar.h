// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
* Copyright (C) 2014, International Business Machines
* Corporation and others.  All Rights Reserved.
******************************************************************************
* sharedcalendar.h
*/

#ifndef __SHARED_CALENDAR_H__
#define __SHARED_CALENDAR_H__

#include "unicode/utypes.h"
#include "sharedobject.h"
#include "unifiedcache.h"

U_NAMESPACE_BEGIN

class Calendar;

class U_I18N_API SharedCalendar : public SharedObject {
public:
    SharedCalendar(Calendar *calToAdopt) : ptr(calToAdopt) { }
    virtual ~SharedCalendar();
    const Calendar *get() const { return ptr; }
    const Calendar *operator->() const { return ptr; }
    const Calendar &operator*() const { return *ptr; }
private:
    Calendar *ptr;
    SharedCalendar(const SharedCalendar &) = delete;
    SharedCalendar &operator=(const SharedCalendar &) = delete;
};

template<> U_I18N_API
const SharedCalendar *LocaleCacheKey<SharedCalendar>::createObject(
        const void * /*unusedCreationContext*/, UErrorCode &status) const;


U_NAMESPACE_END

#endif
