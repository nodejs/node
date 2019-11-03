// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 2003-2013, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  uarrsort.c
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2003aug04
*   created by: Markus W. Scherer
*
*   Internal function for sorting arrays.
*/

#include "unicode/utypes.h"
#include "cmemory.h"
#include "uarrsort.h"

enum {
    /**
     * "from Knuth"
     *
     * A binary search over 8 items performs 4 comparisons:
     * log2(8)=3 to subdivide, +1 to check for equality.
     * A linear search over 8 items on average also performs 4 comparisons.
     */
    MIN_QSORT=9,
    STACK_ITEM_SIZE=200
};

static constexpr int32_t sizeInMaxAlignTs(int32_t sizeInBytes) {
    return (sizeInBytes + sizeof(max_align_t) - 1) / sizeof(max_align_t);
}

/* UComparator convenience implementations ---------------------------------- */

U_CAPI int32_t U_EXPORT2
uprv_uint16Comparator(const void *context, const void *left, const void *right) {
    (void)context;
    return (int32_t)*(const uint16_t *)left - (int32_t)*(const uint16_t *)right;
}

U_CAPI int32_t U_EXPORT2
uprv_int32Comparator(const void *context, const void *left, const void *right) {
    (void)context;
    return *(const int32_t *)left - *(const int32_t *)right;
}

U_CAPI int32_t U_EXPORT2
uprv_uint32Comparator(const void *context, const void *left, const void *right) {
    (void)context;
    uint32_t l=*(const uint32_t *)left, r=*(const uint32_t *)right;

    /* compare directly because (l-r) would overflow the int32_t result */
    if(l<r) {
        return -1;
    } else if(l==r) {
        return 0;
    } else /* l>r */ {
        return 1;
    }
}

/* Insertion sort using binary search --------------------------------------- */

U_CAPI int32_t U_EXPORT2
uprv_stableBinarySearch(char *array, int32_t limit, void *item, int32_t itemSize,
                        UComparator *cmp, const void *context) {
    int32_t start=0;
    UBool found=FALSE;

    /* Binary search until we get down to a tiny sub-array. */
    while((limit-start)>=MIN_QSORT) {
        int32_t i=(start+limit)/2;
        int32_t diff=cmp(context, item, array+i*itemSize);
        if(diff==0) {
            /*
             * Found the item. We look for the *last* occurrence of such
             * an item, for stable sorting.
             * If we knew that there will be only few equal items,
             * we could break now and enter the linear search.
             * However, if there are many equal items, then it should be
             * faster to continue with the binary search.
             * It seems likely that we either have all unique items
             * (where found will never become TRUE in the insertion sort)
             * or potentially many duplicates.
             */
            found=TRUE;
            start=i+1;
        } else if(diff<0) {
            limit=i;
        } else {
            start=i;
        }
    }

    /* Linear search over the remaining tiny sub-array. */
    while(start<limit) {
        int32_t diff=cmp(context, item, array+start*itemSize);
        if(diff==0) {
            found=TRUE;
        } else if(diff<0) {
            break;
        }
        ++start;
    }
    return found ? (start-1) : ~start;
}

static void
doInsertionSort(char *array, int32_t length, int32_t itemSize,
                UComparator *cmp, const void *context, void *pv) {
    int32_t j;

    for(j=1; j<length; ++j) {
        char *item=array+j*itemSize;
        int32_t insertionPoint=uprv_stableBinarySearch(array, j, item, itemSize, cmp, context);
        if(insertionPoint<0) {
            insertionPoint=~insertionPoint;
        } else {
            ++insertionPoint;  /* one past the last equal item */
        }
        if(insertionPoint<j) {
            char *dest=array+insertionPoint*itemSize;
            uprv_memcpy(pv, item, itemSize);  /* v=array[j] */
            uprv_memmove(dest+itemSize, dest, (j-insertionPoint)*(size_t)itemSize);
            uprv_memcpy(dest, pv, itemSize);  /* array[insertionPoint]=v */
        }
    }
}

static void
insertionSort(char *array, int32_t length, int32_t itemSize,
              UComparator *cmp, const void *context, UErrorCode *pErrorCode) {

    icu::MaybeStackArray<max_align_t, sizeInMaxAlignTs(STACK_ITEM_SIZE)> v;
    if (sizeInMaxAlignTs(itemSize) > v.getCapacity() &&
            v.resize(sizeInMaxAlignTs(itemSize)) == nullptr) {
        *pErrorCode = U_MEMORY_ALLOCATION_ERROR;
        return;
    }

    doInsertionSort(array, length, itemSize, cmp, context, v.getAlias());
}

/* QuickSort ---------------------------------------------------------------- */

/*
 * This implementation is semi-recursive:
 * It recurses for the smaller sub-array to shorten the recursion depth,
 * and loops for the larger sub-array.
 *
 * Loosely after QuickSort algorithms in
 * Niklaus Wirth
 * Algorithmen und Datenstrukturen mit Modula-2
 * B.G. Teubner Stuttgart
 * 4. Auflage 1986
 * ISBN 3-519-02260-5
 */
static void
subQuickSort(char *array, int32_t start, int32_t limit, int32_t itemSize,
             UComparator *cmp, const void *context,
             void *px, void *pw) {
    int32_t left, right;

    /* start and left are inclusive, limit and right are exclusive */
    do {
        if((start+MIN_QSORT)>=limit) {
            doInsertionSort(array+start*itemSize, limit-start, itemSize, cmp, context, px);
            break;
        }

        left=start;
        right=limit;

        /* x=array[middle] */
        uprv_memcpy(px, array+(size_t)((start+limit)/2)*itemSize, itemSize);

        do {
            while(/* array[left]<x */
                  cmp(context, array+left*itemSize, px)<0
            ) {
                ++left;
            }
            while(/* x<array[right-1] */
                  cmp(context, px, array+(right-1)*itemSize)<0
            ) {
                --right;
            }

            /* swap array[left] and array[right-1] via w; ++left; --right */
            if(left<right) {
                --right;

                if(left<right) {
                    uprv_memcpy(pw, array+(size_t)left*itemSize, itemSize);
                    uprv_memcpy(array+(size_t)left*itemSize, array+(size_t)right*itemSize, itemSize);
                    uprv_memcpy(array+(size_t)right*itemSize, pw, itemSize);
                }

                ++left;
            }
        } while(left<right);

        /* sort sub-arrays */
        if((right-start)<(limit-left)) {
            /* sort [start..right[ */
            if(start<(right-1)) {
                subQuickSort(array, start, right, itemSize, cmp, context, px, pw);
            }

            /* sort [left..limit[ */
            start=left;
        } else {
            /* sort [left..limit[ */
            if(left<(limit-1)) {
                subQuickSort(array, left, limit, itemSize, cmp, context, px, pw);
            }

            /* sort [start..right[ */
            limit=right;
        }
    } while(start<(limit-1));
}

static void
quickSort(char *array, int32_t length, int32_t itemSize,
            UComparator *cmp, const void *context, UErrorCode *pErrorCode) {
    /* allocate two intermediate item variables (x and w) */
    icu::MaybeStackArray<max_align_t, sizeInMaxAlignTs(STACK_ITEM_SIZE) * 2> xw;
    if(sizeInMaxAlignTs(itemSize)*2 > xw.getCapacity() &&
            xw.resize(sizeInMaxAlignTs(itemSize) * 2) == nullptr) {
        *pErrorCode=U_MEMORY_ALLOCATION_ERROR;
        return;
    }

    subQuickSort(array, 0, length, itemSize, cmp, context,
                 xw.getAlias(), xw.getAlias() + sizeInMaxAlignTs(itemSize));
}

/* uprv_sortArray() API ----------------------------------------------------- */

/*
 * Check arguments, select an appropriate implementation,
 * cast the array to char * so that array+i*itemSize works.
 */
U_CAPI void U_EXPORT2
uprv_sortArray(void *array, int32_t length, int32_t itemSize,
               UComparator *cmp, const void *context,
               UBool sortStable, UErrorCode *pErrorCode) {
    if(pErrorCode==NULL || U_FAILURE(*pErrorCode)) {
        return;
    }
    if((length>0 && array==NULL) || length<0 || itemSize<=0 || cmp==NULL) {
        *pErrorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }

    if(length<=1) {
        return;
    } else if(length<MIN_QSORT || sortStable) {
        insertionSort((char *)array, length, itemSize, cmp, context, pErrorCode);
    } else {
        quickSort((char *)array, length, itemSize, cmp, context, pErrorCode);
    }
}
