// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2009-2010, International Business Machines Corporation and         *
* others. All Rights Reserved.                                                *
*******************************************************************************
*/

/**
 * \file 
 * \brief C API: Time zone transition classes
 */

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/uobject.h"
#include "ztrans.h"
#include "unicode/tztrans.h"
#include "cmemory.h"
#include "unicode/ustring.h"
#include "unicode/parsepos.h"

U_NAMESPACE_USE

U_CAPI ZTrans* U_EXPORT2
ztrans_open(UDate time, const void* from, const void* to){
    return (ZTrans*) new TimeZoneTransition(time,*(TimeZoneRule*)from,*(TimeZoneRule*)to);
}

U_CAPI ZTrans* U_EXPORT2
ztrans_openEmpty() {
    return (ZTrans*) new TimeZoneTransition();
}

U_CAPI void U_EXPORT2
ztrans_close(ZTrans *trans) {
    delete (TimeZoneTransition*)trans;
}

U_CAPI ZTrans* U_EXPORT2
ztrans_clone(ZTrans *trans) {
    return (ZTrans*) (((TimeZoneTransition*)trans)->TimeZoneTransition::clone());
}

U_CAPI UBool U_EXPORT2
ztrans_equals(const ZTrans* trans1, const ZTrans* trans2){
    return *(const TimeZoneTransition*)trans1 == *(const TimeZoneTransition*)trans2;
}

U_CAPI UDate U_EXPORT2
ztrans_getTime(ZTrans* trans) {
    return ((TimeZoneTransition*)trans)->TimeZoneTransition::getTime();
}

U_CAPI void U_EXPORT2
ztrans_setTime(ZTrans* trans, UDate time) {
    return ((TimeZoneTransition*)trans)->TimeZoneTransition::setTime(time);
}

U_CAPI void* U_EXPORT2
ztrans_getFrom(ZTrans* & trans) {
    return (void*) (((TimeZoneTransition*)trans)->TimeZoneTransition::getFrom());
}

U_CAPI void U_EXPORT2
ztrans_setFrom(ZTrans* trans, const void* from) {
    return ((TimeZoneTransition*)trans)->TimeZoneTransition::setFrom(*(TimeZoneRule*)from);
}

U_CAPI void U_EXPORT2
ztrans_adoptFrom(ZTrans* trans, void* from) {
    return ((TimeZoneTransition*)trans)->TimeZoneTransition::adoptFrom((TimeZoneRule*)from);
}

U_CAPI void* U_EXPORT2
ztrans_getTo(ZTrans* trans){
    return (void*) (((TimeZoneTransition*)trans)->TimeZoneTransition::getTo());
}

U_CAPI void U_EXPORT2
ztrans_setTo(ZTrans* trans, const void* to) {
    return ((TimeZoneTransition*)trans)->TimeZoneTransition::setTo(*(TimeZoneRule*)to);
}

U_CAPI void U_EXPORT2
ztrans_adoptTo(ZTrans* trans, void* to) {
    return ((TimeZoneTransition*)trans)->TimeZoneTransition::adoptTo((TimeZoneRule*)to);
}

U_CAPI UClassID U_EXPORT2
ztrans_getStaticClassID(ZTrans* trans) {
    return ((TimeZoneTransition*)trans)->TimeZoneTransition::getStaticClassID();
}

U_CAPI UClassID U_EXPORT2
ztrans_getDynamicClassID(ZTrans* trans){
    return ((TimeZoneTransition*)trans)->TimeZoneTransition::getDynamicClassID();
}

#endif
