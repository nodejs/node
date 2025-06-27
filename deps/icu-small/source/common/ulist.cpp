// © 2016 and later: Unicode, Inc. and others.
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
};

static void ulist_addFirstItem(UList *list, UListNode *newItem);

U_CAPI UList *U_EXPORT2 ulist_createEmptyList(UErrorCode *status) {
    UList *newList = nullptr;
    
    if (U_FAILURE(*status)) {
        return nullptr;
    }
    
    newList = (UList *)uprv_malloc(sizeof(UList));
    if (newList == nullptr) {
        *status = U_MEMORY_ALLOCATION_ERROR;
        return nullptr;
    }
    
    newList->curr = nullptr;
    newList->head = nullptr;
    newList->tail = nullptr;
    newList->size = 0;
    
    return newList;
}

/*
 * Function called by addItemEndList or addItemBeginList when the first item is added to the list.
 * This function properly sets the pointers for the first item added.
 */
static void ulist_addFirstItem(UList *list, UListNode *newItem) {
    newItem->next = nullptr;
    newItem->previous = nullptr;
    list->head = newItem;
    list->tail = newItem;
}

static void ulist_removeItem(UList *list, UListNode *p) {
    if (p->previous == nullptr) {
        // p is the list head.
        list->head = p->next;
    } else {
        p->previous->next = p->next;
    }
    if (p->next == nullptr) {
        // p is the list tail.
        list->tail = p->previous;
    } else {
        p->next->previous = p->previous;
    }
    if (p == list->curr) {
        list->curr = p->next;
    }
    --list->size;
    if (p->forceDelete) {
        uprv_free(p->data);
    }
    uprv_free(p);
}

U_CAPI void U_EXPORT2 ulist_addItemEndList(UList *list, const void *data, UBool forceDelete, UErrorCode *status) {
    UListNode *newItem = nullptr;
    
    if (U_FAILURE(*status) || list == nullptr || data == nullptr) {
        if (forceDelete) {
            uprv_free((void *)data);
        }
        return;
    }
    
    newItem = (UListNode *)uprv_malloc(sizeof(UListNode));
    if (newItem == nullptr) {
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
        newItem->next = nullptr;
        newItem->previous = list->tail;
        list->tail->next = newItem;
        list->tail = newItem;
    }
    
    list->size++;
}

U_CAPI void U_EXPORT2 ulist_addItemBeginList(UList *list, const void *data, UBool forceDelete, UErrorCode *status) {
    UListNode *newItem = nullptr;
    
    if (U_FAILURE(*status) || list == nullptr || data == nullptr) {
        if (forceDelete) {
            uprv_free((void *)data);
        }
        return;
    }
    
    newItem = (UListNode *)uprv_malloc(sizeof(UListNode));
    if (newItem == nullptr) {
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
        newItem->previous = nullptr;
        newItem->next = list->head;
        list->head->previous = newItem;
        list->head = newItem;
    }
    
    list->size++;
}

U_CAPI UBool U_EXPORT2 ulist_containsString(const UList *list, const char *data, int32_t length) {
    if (list != nullptr) {
        const UListNode *pointer;
        for (pointer = list->head; pointer != nullptr; pointer = pointer->next) {
            if (length == (int32_t)uprv_strlen((const char *)pointer->data)) {
                if (uprv_memcmp(data, pointer->data, length) == 0) {
                    return true;
                }
            }
        }
    }
    return false;
}

U_CAPI UBool U_EXPORT2 ulist_removeString(UList *list, const char *data) {
    if (list != nullptr) {
        UListNode *pointer;
        for (pointer = list->head; pointer != nullptr; pointer = pointer->next) {
            if (uprv_strcmp(data, (const char *)pointer->data) == 0) {
                ulist_removeItem(list, pointer);
                // Remove only the first occurrence, like Java LinkedList.remove(Object).
                return true;
            }
        }
    }
    return false;
}

U_CAPI void *U_EXPORT2 ulist_getNext(UList *list) {
    UListNode *curr = nullptr;
    
    if (list == nullptr || list->curr == nullptr) {
        return nullptr;
    }
    
    curr = list->curr;
    list->curr = curr->next;
    
    return curr->data;
}

U_CAPI int32_t U_EXPORT2 ulist_getListSize(const UList *list) {
    if (list != nullptr) {
        return list->size;
    }
    
    return -1;
}

U_CAPI void U_EXPORT2 ulist_resetList(UList *list) {
    if (list != nullptr) {
        list->curr = list->head;
    }
}

U_CAPI void U_EXPORT2 ulist_deleteList(UList *list) {
    UListNode *listHead = nullptr;

    if (list != nullptr) {
        listHead = list->head;
        while (listHead != nullptr) {
            UListNode *listPointer = listHead->next;

            if (listHead->forceDelete) {
                uprv_free(listHead->data);
            }

            uprv_free(listHead);
            listHead = listPointer;
        }
        uprv_free(list);
        list = nullptr;
    }
}

U_CAPI void U_EXPORT2 ulist_close_keyword_values_iterator(UEnumeration *en) {
    if (en != nullptr) {
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
        return nullptr;
    }

    s = (const char *)ulist_getNext((UList *)(en->context));
    if (s != nullptr && resultLength != nullptr) {
        *resultLength = static_cast<int32_t>(uprv_strlen(s));
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
