// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
*   Copyright (C) 2009-2016, International Business Machines
*   Corporation and others.  All Rights Reserved.
******************************************************************************
*/

#include "ulist.h"
#include "cmemory.h"
#include "cstring.h"
#include "uenumimp.h"

typedef struct UListNode UListNode;
struct UListNode {
    void *data;

    UListNode *next;
    UListNode *previous;

    /* When data is created with uprv_malloc, needs to be freed during deleteList function. */
    UBool forceDelete;
};

struct UList {
    UListNode *curr;
    UListNode *head;
    UListNode *tail;

    int32_t size;
    int32_t currentIndex;
};

static void ulist_addFirstItem(UList *list, UListNode *newItem);

U_CAPI UList *U_EXPORT2 ulist_createEmptyList(UErrorCode *status) {
    UList *newList = NULL;

    if (U_FAILURE(*status)) {
        return NULL;
    }

    newList = (UList *)uprv_malloc(sizeof(UList));
    if (newList == NULL) {
        *status = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }

    newList->curr = NULL;
    newList->head = NULL;
    newList->tail = NULL;
    newList->size = 0;
    newList->currentIndex = -1;

    return newList;
}

/*
 * Function called by addItemEndList or addItemBeginList when the first item is added to the list.
 * This function properly sets the pointers for the first item added.
 */
static void ulist_addFirstItem(UList *list, UListNode *newItem) {
    newItem->next = NULL;
    newItem->previous = NULL;
    list->head = newItem;
    list->tail = newItem;
}

static void ulist_removeItem(UList *list, UListNode *p) {
    if (p->previous == NULL) {
        // p is the list head.
        list->head = p->next;
    } else {
        p->previous->next = p->next;
    }
    if (p->next == NULL) {
        // p is the list tail.
        list->tail = p->previous;
    } else {
        p->next->previous = p->previous;
    }
    list->curr = NULL;
    list->currentIndex = 0;
    --list->size;
    if (p->forceDelete) {
        uprv_free(p->data);
    }
    uprv_free(p);
}

U_CAPI void U_EXPORT2 ulist_addItemEndList(UList *list, const void *data, UBool forceDelete, UErrorCode *status) {
    UListNode *newItem = NULL;

    if (U_FAILURE(*status) || list == NULL || data == NULL) {
        if (forceDelete) {
            uprv_free((void *)data);
        }
        return;
    }

    newItem = (UListNode *)uprv_malloc(sizeof(UListNode));
    if (newItem == NULL) {
        if (forceDelete) {
            uprv_free((void *)data);
        }
        *status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    newItem->data = (void *)(data);
    newItem->forceDelete = forceDelete;

    if (list->size == 0) {
        ulist_addFirstItem(list, newItem);
    } else {
        newItem->next = NULL;
        newItem->previous = list->tail;
        list->tail->next = newItem;
        list->tail = newItem;
    }

    list->size++;
}

U_CAPI void U_EXPORT2 ulist_addItemBeginList(UList *list, const void *data, UBool forceDelete, UErrorCode *status) {
    UListNode *newItem = NULL;

    if (U_FAILURE(*status) || list == NULL || data == NULL) {
        if (forceDelete) {
            uprv_free((void *)data);
        }
        return;
    }

    newItem = (UListNode *)uprv_malloc(sizeof(UListNode));
    if (newItem == NULL) {
        if (forceDelete) {
            uprv_free((void *)data);
        }
        *status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    newItem->data = (void *)(data);
    newItem->forceDelete = forceDelete;

    if (list->size == 0) {
        ulist_addFirstItem(list, newItem);
    } else {
        newItem->previous = NULL;
        newItem->next = list->head;
        list->head->previous = newItem;
        list->head = newItem;
        list->currentIndex++;
    }

    list->size++;
}

U_CAPI UBool U_EXPORT2 ulist_containsString(const UList *list, const char *data, int32_t length) {
    if (list != NULL) {
        const UListNode *pointer;
        for (pointer = list->head; pointer != NULL; pointer = pointer->next) {
            if (length == uprv_strlen(pointer->data)) {
                if (uprv_memcmp(data, pointer->data, length) == 0) {
                    return TRUE;
                }
            }
        }
    }
    return FALSE;
}

U_CAPI UBool U_EXPORT2 ulist_removeString(UList *list, const char *data) {
    if (list != NULL) {
        UListNode *pointer;
        for (pointer = list->head; pointer != NULL; pointer = pointer->next) {
            if (uprv_strcmp(data, pointer->data) == 0) {
                ulist_removeItem(list, pointer);
                // Remove only the first occurrence, like Java LinkedList.remove(Object).
                return TRUE;
            }
        }
    }
    return FALSE;
}

U_CAPI void *U_EXPORT2 ulist_getNext(UList *list) {
    UListNode *curr = NULL;

    if (list == NULL || list->curr == NULL) {
        return NULL;
    }

    curr = list->curr;
    list->curr = curr->next;
    list->currentIndex++;

    return curr->data;
}

U_CAPI int32_t U_EXPORT2 ulist_getListSize(const UList *list) {
    if (list != NULL) {
        return list->size;
    }

    return -1;
}

U_CAPI void U_EXPORT2 ulist_resetList(UList *list) {
    if (list != NULL) {
        list->curr = list->head;
        list->currentIndex = 0;
    }
}

U_CAPI void U_EXPORT2 ulist_deleteList(UList *list) {
    UListNode *listHead = NULL;

    if (list != NULL) {
        listHead = list->head;
        while (listHead != NULL) {
            UListNode *listPointer = listHead->next;

            if (listHead->forceDelete) {
                uprv_free(listHead->data);
            }

            uprv_free(listHead);
            listHead = listPointer;
        }
        uprv_free(list);
        list = NULL;
    }
}

U_CAPI void U_EXPORT2 ulist_close_keyword_values_iterator(UEnumeration *en) {
    if (en != NULL) {
        ulist_deleteList((UList *)(en->context));
        uprv_free(en);
    }
}

U_CAPI int32_t U_EXPORT2 ulist_count_keyword_values(UEnumeration *en, UErrorCode *status) {
    if (U_FAILURE(*status)) {
        return -1;
    }

    return ulist_getListSize((UList *)(en->context));
}

U_CAPI const char * U_EXPORT2 ulist_next_keyword_value(UEnumeration *en, int32_t *resultLength, UErrorCode *status) {
    const char *s;
    if (U_FAILURE(*status)) {
        return NULL;
    }

    s = (const char *)ulist_getNext((UList *)(en->context));
    if (s != NULL && resultLength != NULL) {
        *resultLength = uprv_strlen(s);
    }
    return s;
}

U_CAPI void U_EXPORT2 ulist_reset_keyword_values_iterator(UEnumeration *en, UErrorCode *status) {
    if (U_FAILURE(*status)) {
        return ;
    }

    ulist_resetList((UList *)(en->context));
}

U_CAPI UList * U_EXPORT2 ulist_getListFromEnum(UEnumeration *en) {
    return (UList *)(en->context);
}
