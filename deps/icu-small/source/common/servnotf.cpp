// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/**
 *******************************************************************************
 * Copyright (C) 2001-2012, International Business Machines Corporation and    *
 * others. All Rights Reserved.                                                *
 *******************************************************************************
 */

#include "unicode/utypes.h"

#if !UCONFIG_NO_SERVICE

#include "servnotf.h"
#ifdef NOTIFIER_DEBUG
#include <stdio.h>
#endif

U_NAMESPACE_BEGIN

EventListener::~EventListener() {}
UOBJECT_DEFINE_RTTI_IMPLEMENTATION(EventListener)

static UMutex notifyLock;

ICUNotifier::ICUNotifier(void) 
: listeners(NULL) 
{
}

ICUNotifier::~ICUNotifier(void) {
    {
        Mutex lmx(&notifyLock);
        delete listeners;
        listeners = NULL;
    }
}


void 
ICUNotifier::addListener(const EventListener* l, UErrorCode& status) 
{
    if (U_SUCCESS(status)) {
        if (l == NULL) {
            status = U_ILLEGAL_ARGUMENT_ERROR;
            return;
        }

        if (acceptsListener(*l)) {
            Mutex lmx(&notifyLock);
            if (listeners == NULL) {
                LocalPointer<UVector> lpListeners(new UVector(5, status), status);
                if (U_FAILURE(status)) {
                    return;
                }
                listeners = lpListeners.orphan();
            } else {
                for (int i = 0, e = listeners->size(); i < e; ++i) {
                    const EventListener* el = (const EventListener*)(listeners->elementAt(i));
                    if (l == el) {
                        return;
                    }
                }
            }

            listeners->addElement((void*)l, status); // cast away const
        }
#ifdef NOTIFIER_DEBUG
        else {
            fprintf(stderr, "Listener invalid for this notifier.");
            exit(1);
        }
#endif
    }
}

void 
ICUNotifier::removeListener(const EventListener *l, UErrorCode& status) 
{
    if (U_SUCCESS(status)) {
        if (l == NULL) {
            status = U_ILLEGAL_ARGUMENT_ERROR;
            return;
        }

        {
            Mutex lmx(&notifyLock);
            if (listeners != NULL) {
                // identity equality check
                for (int i = 0, e = listeners->size(); i < e; ++i) {
                    const EventListener* el = (const EventListener*)listeners->elementAt(i);
                    if (l == el) {
                        listeners->removeElementAt(i);
                        if (listeners->size() == 0) {
                            delete listeners;
                            listeners = NULL;
                        }
                        return;
                    }
                }
            }
        }
    }
}

void 
ICUNotifier::notifyChanged(void) 
{
    Mutex lmx(&notifyLock);
    if (listeners != NULL) {
        for (int i = 0, e = listeners->size(); i < e; ++i) {
            EventListener* el = (EventListener*)listeners->elementAt(i);
            notifyListener(*el);
        }
    }
}

U_NAMESPACE_END

/* UCONFIG_NO_SERVICE */
#endif

