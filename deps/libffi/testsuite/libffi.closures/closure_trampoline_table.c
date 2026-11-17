/* Area: ffi_closure_alloc, ffi_closure_free
   Purpose: Churn the closure allocator across many live closures.
	    On targets that use the Darwin trampoline-table allocator
	    this spans several trampoline tables and exercises the
	    allocator's internal table list.  Freeing a partially used
	    non-head table moved it to the head of the list without
	    unlinking it first, corrupting the doubly-linked list; a
	    later free then wrote through the stale links into freed
	    table memory, a heap use-after-free (GHSA-23hx-9rxv-9qfp).
	    The sequence is harmless on every other allocator.
   Limitations: Only the Darwin trampoline-table allocator keeps such a
	    list, so elsewhere this is just an allocator stress test.
   PR: GHSA-23hx-9rxv-9qfp  */

/* { dg-do run } */

#include "ffitest.h"

/* Enough closures to span several trampoline tables on the Darwin
   allocator (at most 1024 entries per table), so that the interleaved
   free order below forces non-head tables to be bumped to the front of
   the list and later emptied and freed.  */
#define N 4096

int
main (void)
{
  ffi_closure **closures;
  void *code;
  int round, i;

  closures = malloc (N * sizeof (*closures));
  CHECK (closures != NULL);

  for (round = 0; round < 3; round++)
    {
      for (i = 0; i < N; i++)
	{
	  code = NULL;
	  closures[i] = ffi_closure_alloc (sizeof (ffi_closure), &code);
	  CHECK (closures[i] != NULL);
	  CHECK (code != NULL);
	}

      /* Interleave the frees: the first pass returns one entry from
	 many different tables, bumping partially used non-head tables
	 to the head of the list; the second pass then empties and frees
	 tables while the list is in that state.  Before the fix this
	 walked stale prev/next links and wrote into freed memory.  */
      for (i = 0; i < N; i += 2)
	ffi_closure_free (closures[i]);
      for (i = 1; i < N; i += 2)
	ffi_closure_free (closures[i]);
    }

  free (closures);

  printf ("closure trampoline table ok\n");
  exit (0);
}

/* { dg-output "closure trampoline table ok" } */
