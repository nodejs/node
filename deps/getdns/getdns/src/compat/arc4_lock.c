/* arc4_lock.c - global lock for arc4random
*
 * Copyright (c) 2014, NLnet Labs. All rights reserved.
 *
 * This software is open source.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 
 * Neither the name of the NLNET LABS nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "config.h"
#define LOCKRET(func) func

#ifdef HAVE_PTHREAD
#include "pthread.h"

static pthread_mutex_t arc_lock = PTHREAD_MUTEX_INITIALIZER;

void _ARC4_LOCK(void)
{
    pthread_mutex_lock(&arc_lock);
}

void _ARC4_UNLOCK(void)
{
    pthread_mutex_unlock(&arc_lock);
}
#elif defined(GETDNS_ON_WINDOWS)
 /*
 * There is no explicit arc4random_init call, and thus
 * the critical section must be allocated on the first call to
 * ARC4_LOCK(). The interlocked test is used to verify that
 * the critical section will be allocated only once.
 *
 * The work around is for the main program to call arc4random()
 * at the beginning of execution, before spinning new threads.
 *
 * There is also no explicit arc4random_close call, and thus
 * the critical section is never deleted. It will remain allocated
 * as long as the program runs.
 */
static CRITICAL_SECTION arc_critical_section;
static volatile long arc_critical_section_initialized = 0;

void _ARC4_LOCK(void)
{
	long r = InterlockedCompareExchange(&arc_critical_section_initialized, 1, 0);

	if (r != 2)
	{
		if (r == 0)
		{
			InitializeCriticalSection(&arc_critical_section);
			arc_critical_section_initialized = 2;
		}
		else if (r == 1)
		{
			/*
			* If the critical section is initialized, the first test
			* will return the value 2.
			*
			* If several threads try to initialize the arc4random
			* state "at the same time", the first one will find
			* the "initialized" variable at 0, the other ones at 1.
			*
			* Since this is a fairly rare event, we resolve it with a
			* simple active wait loop.
			*/

			while (arc_critical_section_initialized != 2)
			{
				Sleep(1);
			}
		}
	}

	EnterCriticalSection(&arc_critical_section);
}

void _ARC4_UNLOCK(void)
{
	LeaveCriticalSection(&arc_critical_section);
}

#else
 /* XXX - add non pthread specific lock routines here */
void _ARC4_LOCK(void)
{
}

void _ARC4_UNLOCK(void)
{
}
#endif
