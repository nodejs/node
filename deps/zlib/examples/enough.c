/* enough.c -- determine the maximum size of inflate's Huffman code tables over
 * all possible valid and complete Huffman codes, subject to a length limit.
 * Copyright (C) 2007, 2008, 2012 Mark Adler
 * Version 1.4  18 August 2012  Mark Adler
 */

/* Version history:
   1.0   3 Jan 2007  First version (derived from codecount.c version 1.4)
   1.1   4 Jan 2007  Use faster incremental table usage computation
                     Prune examine() search on previously visited states
   1.2   5 Jan 2007  Comments clean up
                     As inflate does, decrease root for short codes
                     Refuse cases where inflate would increase root
   1.3  17 Feb 2008  Add argument for initial root table size
                     Fix bug for initial root table size == max - 1
                     Use a macro to compute the history index
   1.4  18 Aug 2012  Avoid shifts more than bits in type (caused endless loop!)
                     Clean up comparisons of different types
                     Clean up code indentation
 */

/*
   Examine all possible Huffman codes for a given number of symbols and a
   maximum code length in bits to determine the maximum table size for zilb's
   inflate.  Only complete Huffman codes are counted.

   Two codes are considered distinct if the vectors of the number of codes per
   length are not identical.  So permutations of the symbol assignments result
   in the same code for the counting, as do permutations of the assignments of
   the bit values to the codes (i.e. only canonical codes are counted).

   We build a code from shorter to longer lengths, determining how many symbols
   are coded at each length.  At each step, we have how many symbols remain to
   be coded, what the last code length used was, and how many bit patterns of
   that length remain unused. Then we add one to the code length and double the
   number of unused patterns to graduate to the next code length.  We then
   assign all portions of the remaining symbols to that code length that
   preserve the properties of a correct and eventually complete code.  Those
   properties are: we cannot use more bit patterns than are available; and when
   all the symbols are used, there are exactly zero possible bit patterns
   remaining.

   The inflate Huffman decoding algorithm uses two-level lookup tables for
   speed.  There is a single first-level table to decode codes up to root bits
   in length (root == 9 in the current inflate implementation).  The table
   has 1 << root entries and is indexed by the next root bits of input.  Codes
   shorter than root bits have replicated table entries, so that the correct
   entry is pointed to regardless of the bits that follow the short code.  If
   the code is longer than root bits, then the table entry points to a second-
   level table.  The size of that table is determined by the longest code with
   that root-bit prefix.  If that longest code has length len, then the table
   has size 1 << (len - root), to index the remaining bits in that set of
   codes.  Each subsequent root-bit prefix then has its own sub-table.  The
   total number of table entries required by the code is calculated
   incrementally as the number of codes at each bit length is populated.  When
   all of the codes are shorter than root bits, then root is reduced to the
   longest code length, resulting in a single, smaller, one-level table.

   The inflate algorithm also provides for small values of root (relative to
   the log2 of the number of symbols), where the shortest code has more bits
   than root.  In that case, root is increased to the length of the shortest
   code.  This program, by design, does not handle that case, so it is verified
   that the number of symbols is less than 2^(root + 1).

   In order to speed up the examination (by about ten orders of magnitude for
   the default arguments), the intermediate states in the build-up of a code
   are remembered and previously visited branches are pruned.  The memory
   required for this will increase rapidly with the total number of symbols and
   the maximum code length in bits.  However this is a very small price to pay
   for the vast speedup.

   First, all of the possible Huffman codes are counted, and reachable
   intermediate states are noted by a non-zero count in a saved-results array.
   Second, the intermediate states that lead to (root + 1) bit or longer codes
   are used to look at all sub-codes from those junctures for their inflate
   memory usage.  (The amount of memory used is not affected by the number of
   codes of root bits or less in length.)  Third, the visited states in the
   construction of those sub-codes and the associated calculation of the table
   size is recalled in order to avoid recalculating from the same juncture.
   Beginning the code examination at (root + 1) bit codes, which is enabled by
   identifying the reachable nodes, accounts for about six of the orders of
   magnitude of improvement for the default arguments.  About another four
   orders of magnitude come from not revisiting previous states.  Out of
   approximately 2x10^16 possible Huffman codes, only about 2x10^6 sub-codes
   need to be examined to cover all of the possible table memory usage cases
   for the default arguments of 286 symbols limited to 15-bit codes.

   Note that an unsigned long long type is used for counting.  It is quite easy
   to exceed the capacity of an eight-byte integer with a large number of
   symbols and a large maximum code length, so multiple-precision arithmetic
   would need to replace the unsigned long long arithmetic in that case.  This
   program will abort if an overflow occurs.  The big_t type identifies where
   the counting takes place.

   An unsigned long long type is also used for calculating the number of
   possible codes remaining at the maximum length.  This limits the maximum
   code length to the number of bits in a long long minus the number of bits
   needed to represent the symbols in a flat code.  The code_t type identifies
   where the bit pattern counting takes place.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define local static

/* special data types */
typedef unsigned long long big_t;   /* type for code counting */
typedef unsigned long long code_t;  /* type for bit pattern counting */
struct tab {                        /* type for been here check */
    size_t len;         /* length of bit vector in char's */
    char *vec;          /* allocated bit vector */
};

/* The array for saving results, num[], is indexed with this triplet:

      syms: number of symbols remaining to code
      left: number of available bit patterns at length len
      len: number of bits in the codes currently being assigned

   Those indices are constrained thusly when saving results:

      syms: 3..totsym (totsym == total symbols to code)
      left: 2..syms - 1, but only the evens (so syms == 8 -> 2, 4, 6)
      len: 1..max - 1 (max == maximum code length in bits)

   syms == 2 is not saved since that immediately leads to a single code.  left
   must be even, since it represents the number of available bit patterns at
   the current length, which is double the number at the previous length.
   left ends at syms-1 since left == syms immediately results in a single code.
   (left > sym is not allowed since that would result in an incomplete code.)
   len is less than max, since the code completes immediately when len == max.

   The offset into the array is calculated for the three indices with the
   first one (syms) being outermost, and the last one (len) being innermost.
   We build the array with length max-1 lists for the len index, with syms-3
   of those for each symbol.  There are totsym-2 of those, with each one
   varying in length as a function of sym.  See the calculation of index in
   count() for the index, and the calculation of size in main() for the size
   of the array.

   For the deflate example of 286 symbols limited to 15-bit codes, the array
   has 284,284 entries, taking up 2.17 MB for an 8-byte big_t.  More than
   half of the space allocated for saved results is actually used -- not all
   possible triplets are reached in the generation of valid Huffman codes.
 */

/* The array for tracking visited states, done[], is itself indexed identically
   to the num[] array as described above for the (syms, left, len) triplet.
   Each element in the array is further indexed by the (mem, rem) doublet,
   where mem is the amount of inflate table space used so far, and rem is the
   remaining unused entries in the current inflate sub-table.  Each indexed
   element is simply one bit indicating whether the state has been visited or
   not.  Since the ranges for mem and rem are not known a priori, each bit
   vector is of a variable size, and grows as needed to accommodate the visited
   states.  mem and rem are used to calculate a single index in a triangular
   array.  Since the range of mem is expected in the default case to be about
   ten times larger than the range of rem, the array is skewed to reduce the
   memory usage, with eight times the range for mem than for rem.  See the
   calculations for offset and bit in beenhere() for the details.

   For the deflate example of 286 symbols limited to 15-bit codes, the bit
   vectors grow to total approximately 21 MB, in addition to the 4.3 MB done[]
   array itself.
 */

/* Globals to avoid propagating constants or constant pointers recursively */
local int max;          /* maximum allowed bit length for the codes */
local int root;         /* size of base code table in bits */
local int large;        /* largest code table so far */
local size_t size;      /* number of elements in num and done */
local int *code;        /* number of symbols assigned to each bit length */
local big_t *num;       /* saved results array for code counting */
local struct tab *done; /* states already evaluated array */

/* Index function for num[] and done[] */
#define INDEX(i,j,k) (((size_t)((i-1)>>1)*((i-2)>>1)+(j>>1)-1)*(max-1)+k-1)

/* Free allocated space.  Uses globals code, num, and done. */
local void cleanup(void)
{
    size_t n;

    if (done != NULL) {
        for (n = 0; n < size; n++)
            if (done[n].len)
                free(done[n].vec);
        free(done);
    }
    if (num != NULL)
        free(num);
    if (code != NULL)
        free(code);
}

/* Return the number of possible Huffman codes using bit patterns of lengths
   len through max inclusive, coding syms symbols, with left bit patterns of
   length len unused -- return -1 if there is an overflow in the counting.
   Keep a record of previous results in num to prevent repeating the same
   calculation.  Uses the globals max and num. */
local big_t count(int syms, int len, int left)
{
    big_t sum;          /* number of possible codes from this juncture */
    big_t got;          /* value returned from count() */
    int least;          /* least number of syms to use at this juncture */
    int most;           /* most number of syms to use at this juncture */
    int use;            /* number of bit patterns to use in next call */
    size_t index;       /* index of this case in *num */

    /* see if only one possible code */
    if (syms == left)
        return 1;

    /* note and verify the expected state */
    assert(syms > left && left > 0 && len < max);

    /* see if we've done this one already */
    index = INDEX(syms, left, len);
    got = num[index];
    if (got)
        return got;         /* we have -- return the saved result */

    /* we need to use at least this many bit patterns so that the code won't be
       incomplete at the next length (more bit patterns than symbols) */
    least = (left << 1) - syms;
    if (least < 0)
        least = 0;

    /* we can use at most this many bit patterns, lest there not be enough
       available for the remaining symbols at the maximum length (if there were
       no limit to the code length, this would become: most = left - 1) */
    most = (((code_t)left << (max - len)) - syms) /
            (((code_t)1 << (max - len)) - 1);

    /* count all possible codes from this juncture and add them up */
    sum = 0;
    for (use = least; use <= most; use++) {
        got = count(syms - use, len + 1, (left - use) << 1);
        sum += got;
        if (got == (big_t)0 - 1 || sum < got)   /* overflow */
            return (big_t)0 - 1;
    }

    /* verify that all recursive calls are productive */
    assert(sum != 0);

    /* save the result and return it */
    num[index] = sum;
    return sum;
}

/* Return true if we've been here before, set to true if not.  Set a bit in a
   bit vector to indicate visiting this state.  Each (syms,len,left) state
   has a variable size bit vector indexed by (mem,rem).  The bit vector is
   lengthened if needed to allow setting the (mem,rem) bit. */
local int beenhere(int syms, int len, int left, int mem, int rem)
{
    size_t index;       /* index for this state's bit vector */
    size_t offset;      /* offset in this state's bit vector */
    int bit;            /* mask for this state's bit */
    size_t length;      /* length of the bit vector in bytes */
    char *vector;       /* new or enlarged bit vector */

    /* point to vector for (syms,left,len), bit in vector for (mem,rem) */
    index = INDEX(syms, left, len);
    mem -= 1 << root;
    offset = (mem >> 3) + rem;
    offset = ((offset * (offset + 1)) >> 1) + rem;
    bit = 1 << (mem & 7);

    /* see if we've been here */
    length = done[index].len;
    if (offset < length && (done[index].vec[offset] & bit) != 0)
        return 1;       /* done this! */

    /* we haven't been here before -- set the bit to show we have now */

    /* see if we need to lengthen the vector in order to set the bit */
    if (length <= offset) {
        /* if we have one already, enlarge it, zero out the appended space */
        if (length) {
            do {
                length <<= 1;
            } while (length <= offset);
            vector = realloc(done[index].vec, length);
            if (vector != NULL)
                memset(vector + done[index].len, 0, length - done[index].len);
        }

        /* otherwise we need to make a new vector and zero it out */
        else {
            length = 1 << (len - root);
            while (length <= offset)
                length <<= 1;
            vector = calloc(length, sizeof(char));
        }

        /* in either case, bail if we can't get the memory */
        if (vector == NULL) {
            fputs("abort: unable to allocate enough memory\n", stderr);
            cleanup();
            exit(1);
        }

        /* install the new vector */
        done[index].len = length;
        done[index].vec = vector;
    }

    /* set the bit */
    done[index].vec[offset] |= bit;
    return 0;
}

/* Examine all possible codes from the given node (syms, len, left).  Compute
   the amount of memory required to build inflate's decoding tables, where the
   number of code structures used so far is mem, and the number remaining in
   the current sub-table is rem.  Uses the globals max, code, root, large, and
   done. */
local void examine(int syms, int len, int left, int mem, int rem)
{
    int least;          /* least number of syms to use at this juncture */
    int most;           /* most number of syms to use at this juncture */
    int use;            /* number of bit patterns to use in next call */

    /* see if we have a complete code */
    if (syms == left) {
        /* set the last code entry */
        code[len] = left;

        /* complete computation of memory used by this code */
        while (rem < left) {
            left -= rem;
            rem = 1 << (len - root);
            mem += rem;
        }
        assert(rem == left);

        /* if this is a new maximum, show the entries used and the sub-code */
        if (mem > large) {
            large = mem;
            printf("max %d: ", mem);
            for (use = root + 1; use <= max; use++)
                if (code[use])
                    printf("%d[%d] ", code[use], use);
            putchar('\n');
            fflush(stdout);
        }

        /* remove entries as we drop back down in the recursion */
        code[len] = 0;
        return;
    }

    /* prune the tree if we can */
    if (beenhere(syms, len, left, mem, rem))
        return;

    /* we need to use at least this many bit patterns so that the code won't be
       incomplete at the next length (more bit patterns than symbols) */
    least = (left << 1) - syms;
    if (least < 0)
        least = 0;

    /* we can use at most this many bit patterns, lest there not be enough
       available for the remaining symbols at the maximum length (if there were
       no limit to the code length, this would become: most = left - 1) */
    most = (((code_t)left << (max - len)) - syms) /
            (((code_t)1 << (max - len)) - 1);

    /* occupy least table spaces, creating new sub-tables as needed */
    use = least;
    while (rem < use) {
        use -= rem;
        rem = 1 << (len - root);
        mem += rem;
    }
    rem -= use;

    /* examine codes from here, updating table space as we go */
    for (use = least; use <= most; use++) {
        code[len] = use;
        examine(syms - use, len + 1, (left - use) << 1,
                mem + (rem ? 1 << (len - root) : 0), rem << 1);
        if (rem == 0) {
            rem = 1 << (len - root);
            mem += rem;
        }
        rem--;
    }

    /* remove entries as we drop back down in the recursion */
    code[len] = 0;
}

/* Look at all sub-codes starting with root + 1 bits.  Look at only the valid
   intermediate code states (syms, left, len).  For each completed code,
   calculate the amount of memory required by inflate to build the decoding
   tables. Find the maximum amount of memory required and show the code that
   requires that maximum.  Uses the globals max, root, and num. */
local void enough(int syms)
{
    int n;              /* number of remaing symbols for this node */
    int left;           /* number of unused bit patterns at this length */
    size_t index;       /* index of this case in *num */

    /* clear code */
    for (n = 0; n <= max; n++)
        code[n] = 0;

    /* look at all (root + 1) bit and longer codes */
    large = 1 << root;              /* base table */
    if (root < max)                 /* otherwise, there's only a base table */
        for (n = 3; n <= syms; n++)
            for (left = 2; left < n; left += 2)
            {
                /* look at all reachable (root + 1) bit nodes, and the
                   resulting codes (complete at root + 2 or more) */
                index = INDEX(n, left, root + 1);
                if (root + 1 < max && num[index])       /* reachable node */
                    examine(n, root + 1, left, 1 << root, 0);

                /* also look at root bit codes with completions at root + 1
                   bits (not saved in num, since complete), just in case */
                if (num[index - 1] && n <= left << 1)
                    examine((n - left) << 1, root + 1, (n - left) << 1,
                            1 << root, 0);
            }

    /* done */
    printf("done: maximum of %d table entries\n", large);
}

/*
   Examine and show the total number of possible Huffman codes for a given
   maximum number of symbols, initial root table size, and maximum code length
   in bits -- those are the command arguments in that order.  The default
   values are 286, 9, and 15 respectively, for the deflate literal/length code.
   The possible codes are counted for each number of coded symbols from two to
   the maximum.  The counts for each of those and the total number of codes are
   shown.  The maximum number of inflate table entires is then calculated
   across all possible codes.  Each new maximum number of table entries and the
   associated sub-code (starting at root + 1 == 10 bits) is shown.

   To count and examine Huffman codes that are not length-limited, provide a
   maximum length equal to the number of symbols minus one.

   For the deflate literal/length code, use "enough".  For the deflate distance
   code, use "enough 30 6".

   This uses the %llu printf format to print big_t numbers, which assumes that
   big_t is an unsigned long long.  If the big_t type is changed (for example
   to a multiple precision type), the method of printing will also need to be
   updated.
 */
int main(int argc, char **argv)
{
    int syms;           /* total number of symbols to code */
    int n;              /* number of symbols to code for this run */
    big_t got;          /* return value of count() */
    big_t sum;          /* accumulated number of codes over n */
    code_t word;        /* for counting bits in code_t */

    /* set up globals for cleanup() */
    code = NULL;
    num = NULL;
    done = NULL;

    /* get arguments -- default to the deflate literal/length code */
    syms = 286;
    root = 9;
    max = 15;
    if (argc > 1) {
        syms = atoi(argv[1]);
        if (argc > 2) {
            root = atoi(argv[2]);
            if (argc > 3)
                max = atoi(argv[3]);
        }
    }
    if (argc > 4 || syms < 2 || root < 1 || max < 1) {
        fputs("invalid arguments, need: [sym >= 2 [root >= 1 [max >= 1]]]\n",
              stderr);
        return 1;
    }

    /* if not restricting the code length, the longest is syms - 1 */
    if (max > syms - 1)
        max = syms - 1;

    /* determine the number of bits in a code_t */
    for (n = 0, word = 1; word; n++, word <<= 1)
        ;

    /* make sure that the calculation of most will not overflow */
    if (max > n || (code_t)(syms - 2) >= (((code_t)0 - 1) >> (max - 1))) {
        fputs("abort: code length too long for internal types\n", stderr);
        return 1;
    }

    /* reject impossible code requests */
    if ((code_t)(syms - 1) > ((code_t)1 << max) - 1) {
        fprintf(stderr, "%d symbols cannot be coded in %d bits\n",
                syms, max);
        return 1;
    }

    /* allocate code vector */
    code = calloc(max + 1, sizeof(int));
    if (code == NULL) {
        fputs("abort: unable to allocate enough memory\n", stderr);
        return 1;
    }

    /* determine size of saved results array, checking for overflows,
       allocate and clear the array (set all to zero with calloc()) */
    if (syms == 2)              /* iff max == 1 */
        num = NULL;             /* won't be saving any results */
    else {
        size = syms >> 1;
        if (size > ((size_t)0 - 1) / (n = (syms - 1) >> 1) ||
                (size *= n, size > ((size_t)0 - 1) / (n = max - 1)) ||
                (size *= n, size > ((size_t)0 - 1) / sizeof(big_t)) ||
                (num = calloc(size, sizeof(big_t))) == NULL) {
            fputs("abort: unable to allocate enough memory\n", stderr);
            cleanup();
            return 1;
        }
    }

    /* count possible codes for all numbers of symbols, add up counts */
    sum = 0;
    for (n = 2; n <= syms; n++) {
        got = count(n, 1, 2);
        sum += got;
        if (got == (big_t)0 - 1 || sum < got) {     /* overflow */
            fputs("abort: can't count that high!\n", stderr);
            cleanup();
            return 1;
        }
        printf("%llu %d-codes\n", got, n);
    }
    printf("%llu total codes for 2 to %d symbols", sum, syms);
    if (max < syms - 1)
        printf(" (%d-bit length limit)\n", max);
    else
        puts(" (no length limit)");

    /* allocate and clear done array for beenhere() */
    if (syms == 2)
        done = NULL;
    else if (size > ((size_t)0 - 1) / sizeof(struct tab) ||
             (done = calloc(size, sizeof(struct tab))) == NULL) {
        fputs("abort: unable to allocate enough memory\n", stderr);
        cleanup();
        return 1;
    }

    /* find and show maximum inflate table usage */
    if (root > max)                 /* reduce root to max length */
        root = max;
    if ((code_t)syms < ((code_t)1 << (root + 1)))
        enough(syms);
    else
        puts("cannot handle minimum code lengths > root");

    /* done */
    cleanup();
    return 0;
}
