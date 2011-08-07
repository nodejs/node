README.CV -- Condition Variables
--------------------------------

The original implementation of condition variables in
pthreads-win32 was based on a discussion paper:

"Strategies for Implementing POSIX Condition Variables
on Win32": http://www.cs.wustl.edu/~schmidt/win32-cv-1.html

The changes suggested below were made on Feb 6 2001. This
file is included in the package for the benefit of anyone
interested in understanding the pthreads-win32 implementation
of condition variables and the (sometimes subtle) issues that
it attempts to resolve.

Thanks go to the individuals whose names appear throughout
the following text.

Ross Johnson

--------------------------------------------------------------------

fyi.. (more detailed problem description/demos + possible fix/patch)

regards,
alexander.


Alexander Terekhov
31.01.2001 17:43

To:   ace-bugs@cs.wustl.edu
cc:
From: Alexander Terekhov/Germany/IBM@IBMDE
Subject:  Implementation of POSIX CVs: spur.wakeups/lost
      signals/deadlocks/unfairness



    ACE VERSION:

        5.1.12 (pthread-win32 snapshot 2000-12-29)

    HOST MACHINE and OPERATING SYSTEM:

        IBM IntelliStation Z Pro, 2 x XEON 1GHz, Win2K

    TARGET MACHINE and OPERATING SYSTEM, if different from HOST:
    COMPILER NAME AND VERSION (AND PATCHLEVEL):

        Microsoft Visual C++ 6.0

    AREA/CLASS/EXAMPLE AFFECTED:

        Implementation of POSIX condition variables - OS.cpp/.h

    DOES THE PROBLEM AFFECT:

        EXECUTION? YES!

    SYNOPSIS:

        a) spurious wakeups (minor problem)
        b) lost signals
        c) broadcast deadlock
        d) unfairness (minor problem)

    DESCRIPTION:

        Please see attached copy of discussion thread
        from comp.programming.threads for more details on
        some reported problems. (i've also posted a "fyi"
        message to ace-users a week or two ago but
        unfortunately did not get any response so far).

        It seems that current implementation suffers from
        two essential problems:

        1) cond.waiters_count does not accurately reflect
           number of waiters blocked on semaphore - w/o
           proper synchronisation that could result (in the
           time window when counter is not accurate)
           in spurious wakeups organised by subsequent
           _signals  and _broadcasts.

        2) Always having (with no e.g. copy_and_clear/..)
           the same queue in use (semaphore+counter)
           neither signal nor broadcast provide 'atomic'
           behaviour with respect to other threads/subsequent
           calls to signal/broadcast/wait.

        Each problem and combination of both could produce
        various nasty things:

        a) spurious wakeups (minor problem)

             it is possible that waiter(s) which was already
             unblocked even so is still counted as blocked
             waiter. signal and broadcast will release
             semaphore which will produce a spurious wakeup
             for a 'real' waiter coming later.

        b) lost signals

             signalling thread ends up consuming its own
             signal. please see demo/discussion below.

        c) broadcast deadlock

             last_waiter processing code does not correctly
             handle the case with multiple threads
             waiting for the end of broadcast.
             please see demo/discussion below.

        d) unfairness (minor problem)

             without SignalObjectAndWait some waiter(s)
             may end up consuming broadcasted signals
             multiple times (spurious wakeups) because waiter
             thread(s) can be preempted before they call
             semaphore wait (but after count++ and mtx.unlock).

    REPEAT BY:

        See below... run problem demos programs (tennis.cpp and
        tennisb.cpp) number of times concurrently (on multiprocessor)
        and in multiple sessions or just add a couple of "Sleep"s
        as described in the attached copy of discussion thread
        from comp.programming.threads

    SAMPLE FIX/WORKAROUND:

        See attached patch to pthread-win32.. well, I can not
        claim that it is completely bug free but at least my
        test and tests provided by pthreads-win32 seem to work.
        Perhaps that will help.

        regards,
        alexander.


>> Forum: comp.programming.threads
>> Thread: pthread_cond_* implementation questions
.
.
.
David Schwartz <davids@webmaster.com> wrote:

> terekhov@my-deja.com wrote:
>
>> BTW, could you please also share your view on other perceived
>> "problems" such as nested broadcast deadlock, spurious wakeups
>> and (the latest one) lost signals??
>
>I'm not sure what you mean. The standard allows an implementation
>to do almost whatever it likes. In fact, you could implement
>pthread_cond_wait by releasing the mutex, sleeping a random
>amount of time, and then reacquiring the mutex. Of course,
>this would be a pretty poor implementation, but any code that
>didn't work under that implementation wouldn't be strictly
>compliant.

The implementation you suggested is indeed correct
one (yes, now I see it :). However it requires from
signal/broadcast nothing more than to "{ return 0; }"
That is not the case for pthread-win32 and ACE
implementations. I do think that these implementations
(basically the same implementation) have some serious
problems with wait/signal/broadcast calls. I am looking
for help to clarify whether these problems are real
or not. I think that I can demonstrate what I mean
using one or two small sample programs.
.
.
.
==========
tennis.cpp
==========

#include "ace/Synch.h"
#include "ace/Thread.h"

enum GAME_STATE {

  START_GAME,
  PLAYER_A,     // Player A playes the ball
  PLAYER_B,     // Player B playes the ball
  GAME_OVER,
  ONE_PLAYER_GONE,
  BOTH_PLAYERS_GONE

};

enum GAME_STATE             eGameState;
ACE_Mutex*                  pmtxGameStateLock;
ACE_Condition< ACE_Mutex >* pcndGameStateChange;

void*
  playerA(
    void* pParm
  )
{

  // For access to game state variable
  pmtxGameStateLock->acquire();

  // Play loop
  while ( eGameState < GAME_OVER ) {

    // Play the ball
    cout << endl << "PLAYER-A" << endl;

    // Now its PLAYER-B's turn
    eGameState = PLAYER_B;

    // Signal to PLAYER-B that now it is his turn
    pcndGameStateChange->signal();

    // Wait until PLAYER-B finishes playing the ball
    do {

      pcndGameStateChange->wait();

      if ( PLAYER_B == eGameState )
        cout << endl << "----PLAYER-A: SPURIOUS WAKEUP!!!" << endl;

    } while ( PLAYER_B == eGameState );

  }

  // PLAYER-A gone
  eGameState = (GAME_STATE)(eGameState+1);
  cout << endl << "PLAYER-A GONE" << endl;

  // No more access to state variable needed
  pmtxGameStateLock->release();

  // Signal PLAYER-A gone event
  pcndGameStateChange->broadcast();

  return 0;

}

void*
  playerB(
    void* pParm
  )
{

  // For access to game state variable
  pmtxGameStateLock->acquire();

  // Play loop
  while ( eGameState < GAME_OVER ) {

    // Play the ball
    cout << endl << "PLAYER-B" << endl;

    // Now its PLAYER-A's turn
    eGameState = PLAYER_A;

    // Signal to PLAYER-A that now it is his turn
    pcndGameStateChange->signal();

    // Wait until PLAYER-A finishes playing the ball
    do {

      pcndGameStateChange->wait();

      if ( PLAYER_A == eGameState )
        cout << endl << "----PLAYER-B: SPURIOUS WAKEUP!!!" << endl;

    } while ( PLAYER_A == eGameState );

  }

  // PLAYER-B gone
  eGameState = (GAME_STATE)(eGameState+1);
  cout << endl << "PLAYER-B GONE" << endl;

  // No more access to state variable needed
  pmtxGameStateLock->release();

  // Signal PLAYER-B gone event
  pcndGameStateChange->broadcast();

  return 0;

}


int
main (int, ACE_TCHAR *[])
{

  pmtxGameStateLock   = new ACE_Mutex();
  pcndGameStateChange = new ACE_Condition< ACE_Mutex >( *pmtxGameStateLock
);

  // Set initial state
  eGameState = START_GAME;

  // Create players
  ACE_Thread::spawn( playerA );
  ACE_Thread::spawn( playerB );

  // Give them 5 sec. to play
  Sleep( 5000 );//sleep( 5 );

  // Set game over state
  pmtxGameStateLock->acquire();
  eGameState = GAME_OVER;

  // Let them know
  pcndGameStateChange->broadcast();

  // Wait for players to stop
  do {

    pcndGameStateChange->wait();

  } while ( eGameState < BOTH_PLAYERS_GONE );

  // Cleanup
  cout << endl << "GAME OVER" << endl;
  pmtxGameStateLock->release();
  delete pcndGameStateChange;
  delete pmtxGameStateLock;

  return 0;

}

===========
tennisb.cpp
===========
#include "ace/Synch.h"
#include "ace/Thread.h"

enum GAME_STATE {

  START_GAME,
  PLAYER_A,     // Player A playes the ball
  PLAYER_B,     // Player B playes the ball
  GAME_OVER,
  ONE_PLAYER_GONE,
  BOTH_PLAYERS_GONE

};

enum GAME_STATE             eGameState;
ACE_Mutex*                  pmtxGameStateLock;
ACE_Condition< ACE_Mutex >* pcndGameStateChange;

void*
  playerA(
    void* pParm
  )
{

  // For access to game state variable
  pmtxGameStateLock->acquire();

  // Play loop
  while ( eGameState < GAME_OVER ) {

    // Play the ball
    cout << endl << "PLAYER-A" << endl;

    // Now its PLAYER-B's turn
    eGameState = PLAYER_B;

    // Signal to PLAYER-B that now it is his turn
    pcndGameStateChange->broadcast();

    // Wait until PLAYER-B finishes playing the ball
    do {

      pcndGameStateChange->wait();

      if ( PLAYER_B == eGameState )
        cout << endl << "----PLAYER-A: SPURIOUS WAKEUP!!!" << endl;

    } while ( PLAYER_B == eGameState );

  }

  // PLAYER-A gone
  eGameState = (GAME_STATE)(eGameState+1);
  cout << endl << "PLAYER-A GONE" << endl;

  // No more access to state variable needed
  pmtxGameStateLock->release();

  // Signal PLAYER-A gone event
  pcndGameStateChange->broadcast();

  return 0;

}

void*
  playerB(
    void* pParm
  )
{

  // For access to game state variable
  pmtxGameStateLock->acquire();

  // Play loop
  while ( eGameState < GAME_OVER ) {

    // Play the ball
    cout << endl << "PLAYER-B" << endl;

    // Now its PLAYER-A's turn
    eGameState = PLAYER_A;

    // Signal to PLAYER-A that now it is his turn
    pcndGameStateChange->broadcast();

    // Wait until PLAYER-A finishes playing the ball
    do {

      pcndGameStateChange->wait();

      if ( PLAYER_A == eGameState )
        cout << endl << "----PLAYER-B: SPURIOUS WAKEUP!!!" << endl;

    } while ( PLAYER_A == eGameState );

  }

  // PLAYER-B gone
  eGameState = (GAME_STATE)(eGameState+1);
  cout << endl << "PLAYER-B GONE" << endl;

  // No more access to state variable needed
  pmtxGameStateLock->release();

  // Signal PLAYER-B gone event
  pcndGameStateChange->broadcast();

  return 0;

}


int
main (int, ACE_TCHAR *[])
{

  pmtxGameStateLock   = new ACE_Mutex();
  pcndGameStateChange = new ACE_Condition< ACE_Mutex >( *pmtxGameStateLock
);

  // Set initial state
  eGameState = START_GAME;

  // Create players
  ACE_Thread::spawn( playerA );
  ACE_Thread::spawn( playerB );

  // Give them 5 sec. to play
  Sleep( 5000 );//sleep( 5 );

  // Make some noise
  pmtxGameStateLock->acquire();
  cout << endl << "---Noise ON..." << endl;
  pmtxGameStateLock->release();
  for ( int i = 0; i < 100000; i++ )
    pcndGameStateChange->broadcast();
  cout << endl << "---Noise OFF" << endl;

  // Set game over state
  pmtxGameStateLock->acquire();
  eGameState = GAME_OVER;
  cout << endl << "---Stopping the game..." << endl;

  // Let them know
  pcndGameStateChange->broadcast();

  // Wait for players to stop
  do {

    pcndGameStateChange->wait();

  } while ( eGameState < BOTH_PLAYERS_GONE );

  // Cleanup
  cout << endl << "GAME OVER" << endl;
  pmtxGameStateLock->release();
  delete pcndGameStateChange;
  delete pmtxGameStateLock;

  return 0;

}
.
.
.
David Schwartz <davids@webmaster.com> wrote:
>> > It's compliant
>>
>> That is really good.
>
>> Tomorrow (I have to go urgently now) I will try to
>> demonstrate the lost-signal "problem" of current
>> pthread-win32 and ACE-(variant w/o SingleObjectAndWait)
>> implementations: players start suddenly drop their balls :-)
>> (with no change in source code).
>
>Signals aren't lost, they're going to the main thread,
>which isn't coded correctly to handle them. Try this:
>
>  // Wait for players to stop
>  do {
>
>    pthread_cond_wait( &cndGameStateChange,&mtxGameStateLock );
>printf("Main thread stole a signal\n");
>
>  } while ( eGameState < BOTH_PLAYERS_GONE );
>
>I bet everytime you thing a signal is lost, you'll see that printf.
>The signal isn't lost, it was stolen by another thread.

well, you can probably loose your bet.. it was indeed stolen
by "another" thread but not the one you seem to think of.

I think that what actually happens is the following:

H:\SA\UXX\pt\PTHREADS\TESTS>tennis3.exe

PLAYER-A

PLAYER-B

----PLAYER-B: SPURIOUS WAKEUP!!!

PLAYER-A GONE

PLAYER-B GONE

GAME OVER

H:\SA\UXX\pt\PTHREADS\TESTS>

here you can see that PLAYER-B after playing his first
ball (which came via signal from PLAYER-A) just dropped
it down. What happened is that his signal to player A
was consumed as spurious wakeup by himself (player B).

The implementation has a problem:

================
waiting threads:
================

{ /** Critical Section

  inc cond.waiters_count

}

  /*
  /* Atomic only if using Win32 SignalObjectAndWait
  /*
  cond.mtx.release

  /*** ^^-- A THREAD WHICH DID SIGNAL MAY ACQUIRE THE MUTEX,
  /***      GO INTO WAIT ON THE SAME CONDITION AND OVERTAKE
  /***      ORIGINAL WAITER(S) CONSUMING ITS OWN SIGNAL!

  cond.sem.wait

Player-A after playing game's initial ball went into
wait (called _wait) but was pre-empted before reaching
wait semaphore. He was counted as waiter but was not
actually waiting/blocked yet.

===============
signal threads:
===============

{ /** Critical Section

  waiters_count = cond.waiters_count

}

  if ( waiters_count != 0 )

    sem.post 1

  endif

Player-B after he received signal/ball from Player A
called _signal. The _signal did see that there was
one waiter blocked on the condition (Player-A) and
released the semaphore.. (but it did not unblock
Player-A because he was not actually blocked).
Player-B thread continued its execution, called _wait,
was counted as second waiter BUT was allowed to slip
through opened semaphore gate (which was opened for
Player-B) and received his own signal. Player B remained
blocked followed by Player A. Deadlock happened which
lasted until main thread came in and said game over.

It seems to me that the implementation fails to
correctly implement the following statement
from specification:

http://www.opengroup.org/
onlinepubs/007908799/xsh/pthread_cond_wait.html

"These functions atomically release mutex and cause
the calling thread to block on the condition variable
cond; atomically here means "atomically with respect
to access by another thread to the mutex and then the
condition variable". That is, if another thread is
able to acquire the mutex after the about-to-block
thread has released it, then a subsequent call to
pthread_cond_signal() or pthread_cond_broadcast()
in that thread behaves as if it were issued after
the about-to-block thread has blocked."

Question: Am I right?

(I produced the program output above by simply
adding ?Sleep( 1 )?:

================
waiting threads:
================

{ /** Critical Section

  inc cond.waiters_count

}

  /*
  /* Atomic only if using Win32 SignalObjectAndWait
  /*
  cond.mtx.release

Sleep( 1 ); // Win32

  /*** ^^-- A THREAD WHICH DID SIGNAL MAY ACQUIRE THE MUTEX,
  /***      GO INTO WAIT ON THE SAME CONDITION AND OVERTAKE
  /***      ORIGINAL WAITER(S) CONSUMING ITS OWN SIGNAL!

  cond.sem.wait

to the source code of pthread-win32 implementation:

http://sources.redhat.com/cgi-bin/cvsweb.cgi/pthreads/
condvar.c?rev=1.36&content-type=text/
x-cvsweb-markup&cvsroot=pthreads-win32


  /*
  * We keep the lock held just long enough to increment the count of
  * waiters by one (above).
  * Note that we can't keep it held across the
  * call to sem_wait since that will deadlock other calls
  * to pthread_cond_signal
  */
  cleanup_args.mutexPtr = mutex;
  cleanup_args.cv = cv;
  cleanup_args.resultPtr = &result;

  pthread_cleanup_push (ptw32_cond_wait_cleanup, (void *)
&cleanup_args);

  if ((result = pthread_mutex_unlock (mutex)) == 0)
    {((result
Sleep( 1 ); // @AT

      /*
      * Wait to be awakened by
      *              pthread_cond_signal, or
      *              pthread_cond_broadcast, or
      *              a timeout
      *
      * Note:
      *      ptw32_sem_timedwait is a cancelation point,
      *      hence providing the
      *      mechanism for making pthread_cond_wait a cancelation
      *      point. We use the cleanup mechanism to ensure we
      *      re-lock the mutex and decrement the waiters count
      *      if we are canceled.
      */
      if (ptw32_sem_timedwait (&(cv->sema), abstime) == -1)         {
          result = errno;
        }
    }

  pthread_cleanup_pop (1);  /* Always cleanup */


BTW, on my system (2 CPUs) I can manage to get
signals lost even without any source code modification
if I run the tennis program many times in different
shell sessions.
.
.
.
David Schwartz <davids@webmaster.com> wrote:
>terekhov@my-deja.com wrote:
>
>> well, it might be that the program is in fact buggy.
>> but you did not show me any bug.
>
>You're right. I was close but not dead on. I was correct, however,
>that the code is buggy because it uses 'pthread_cond_signal' even
>though not any thread waiting on the condition variable can do the
>job. I was wrong in which thread could be waiting on the cv but
>unable to do the job.

Okay, lets change 'pthread_cond_signal' to 'pthread_cond_broadcast'
but also add some noise from main() right before declaring the game
to be over (I need it in order to demonstrate another problem of
pthread-win32/ACE implementations - broadcast deadlock)...
.
.
.
It is my understanding of POSIX conditions,
that on correct implementation added noise
in form of unnecessary broadcasts from main,
should not break the tennis program. The
only 'side effect' of added noise on correct
implementation would be 'spurious wakeups' of
players (in fact they are not spurious,
players just see them as spurious) unblocked,
not by another player but by main before
another player had a chance to acquire the
mutex and change the game state variable:
.
.
.

PLAYER-B

PLAYER-A

---Noise ON...

PLAYER-B

PLAYER-A

.
.
.

PLAYER-B

PLAYER-A

----PLAYER-A: SPURIOUS WAKEUP!!!

PLAYER-B

PLAYER-A

---Noise OFF

PLAYER-B

---Stopping the game...

PLAYER-A GONE

PLAYER-B GONE

GAME OVER

H:\SA\UXX\pt\PTHREADS\TESTS>

On pthread-win32/ACE implementations the
program could stall:

.
.
.

PLAYER-A

PLAYER-B

PLAYER-A

PLAYER-B

PLAYER-A

PLAYER-B

PLAYER-A

PLAYER-B

---Noise ON...

PLAYER-A

---Noise OFF
^C
H:\SA\UXX\pt\PTHREADS\TESTS>


The implementation has problems:

================
waiting threads:
================

{ /** Critical Section

  inc cond.waiters_count

}

  /*
  /* Atomic only if using Win32 SignalObjectAndWait
  /*
  cond.mtx.release
  cond.sem.wait

  /*** ^^-- WAITER CAN BE PREEMPTED AFTER BEING UNBLOCKED...

{ /** Critical Section

  dec cond.waiters_count

  /*** ^^- ...AND BEFORE DECREMENTING THE COUNT (1)

  last_waiter = ( cond.was_broadcast &&
                    cond.waiters_count == 0 )

  if ( last_waiter )

    cond.was_broadcast = FALSE

  endif

}

  if ( last_waiter )

    /*
    /* Atomic only if using Win32 SignalObjectAndWait
    /*
    cond.auto_reset_event_or_sem.post /* Event for Win32
    cond.mtx.acquire

  /*** ^^-- ...AND BEFORE CALL TO mtx.acquire (2)

  /*** ^^-- NESTED BROADCASTS RESULT IN A DEADLOCK


  else

    cond.mtx.acquire

  /*** ^^-- ...AND BEFORE CALL TO mtx.acquire (3)

  endif


==================
broadcast threads:
==================

{ /** Critical Section

  waiters_count = cond.waiters_count

  if ( waiters_count != 0 )

    cond.was_broadcast = TRUE

  endif

}

if ( waiters_count != 0 )

  cond.sem.post waiters_count

  /*** ^^^^^--- SPURIOUS WAKEUPS DUE TO (1)

  cond.auto_reset_event_or_sem.wait /* Event for Win32

  /*** ^^^^^--- DEADLOCK FOR FURTHER BROADCASTS IF THEY
                HAPPEN TO GO INTO WAIT WHILE PREVIOUS
                BROADCAST IS STILL IN PROGRESS/WAITING

endif

a) cond.waiters_count does not accurately reflect
number of waiters blocked on semaphore - that could
result (in the time window when counter is not accurate)
in spurios wakeups organised by subsequent _signals
and _broadcasts. From standard compliance point of view
that is OK but that could be a real problem from
performance/efficiency point of view.

b) If subsequent broadcast happen to go into wait on
cond.auto_reset_event_or_sem before previous
broadcast was unblocked from cond.auto_reset_event_or_sem
by its last waiter, one of two blocked threads will
remain blocked because last_waiter processing code
fails to unblock both threads.

In the situation with tennisb.c the Player-B was put
in a deadlock by noise (broadcast) coming from main
thread. And since Player-B holds the game state
mutex when it calls broadcast, the whole program
stalled: Player-A was deadlocked on mutex and
main thread after finishing with producing the noise
was deadlocked on mutex too (needed to declare the
game over)

(I produced the program output above by simply
adding ?Sleep( 1 )?:

==================
broadcast threads:
==================

{ /** Critical Section

  waiters_count = cond.waiters_count

  if ( waiters_count != 0 )

    cond.was_broadcast = TRUE

  endif

}

if ( waiters_count != 0 )

Sleep( 1 ); //Win32

  cond.sem.post waiters_count

  /*** ^^^^^--- SPURIOUS WAKEUPS DUE TO (1)

  cond.auto_reset_event_or_sem.wait /* Event for Win32

  /*** ^^^^^--- DEADLOCK FOR FURTHER BROADCASTS IF THEY
                HAPPEN TO GO INTO WAIT WHILE PREVIOUS
                BROADCAST IS STILL IN PROGRESS/WAITING

endif

to the source code of pthread-win32 implementation:

http://sources.redhat.com/cgi-bin/cvsweb.cgi/pthreads/
condvar.c?rev=1.36&content-type=text/
x-cvsweb-markup&cvsroot=pthreads-win32

  if (wereWaiters)
    {(wereWaiters)sroot=pthreads-win32eb.cgi/pthreads/Yem...m
      /*
      * Wake up all waiters
      */

Sleep( 1 ); //@AT

#ifdef NEED_SEM

      result = (ptw32_increase_semaphore( &cv->sema, cv->waiters )
                 ? 0
                : EINVAL);

#else /* NEED_SEM */

      result = (ReleaseSemaphore( cv->sema, cv->waiters, NULL )
                 ? 0
                : EINVAL);

#endif /* NEED_SEM */

    }

  (void) pthread_mutex_unlock(&(cv->waitersLock));

  if (wereWaiters && result == 0)
    {(wereWaiters
      /*
       * Wait for all the awakened threads to acquire their part of
       * the counting semaphore
       */

      if (WaitForSingleObject (cv->waitersDone, INFINITE)
          == WAIT_OBJECT_0)
        {
          result = 0;
        }
      else
        {
          result = EINVAL;
        }

    }

  return (result);

}

BTW, on my system (2 CPUs) I can manage to get
the program stalled even without any source code
modification if I run the tennisb program many
times in different shell sessions.

===================
pthread-win32 patch
===================
struct pthread_cond_t_ {
  long            nWaitersBlocked;   /* Number of threads blocked
*/
  long            nWaitersUnblocked; /* Number of threads unblocked
*/
  long            nWaitersToUnblock; /* Number of threads to unblock
*/
  sem_t           semBlockQueue;     /* Queue up threads waiting for the
*/
                                     /*   condition to become signalled
*/
  sem_t           semBlockLock;      /* Semaphore that guards access to
*/
                                     /* | waiters blocked count/block queue
*/
                                     /* +-> Mandatory Sync.LEVEL-1
*/
  pthread_mutex_t mtxUnblockLock;    /* Mutex that guards access to
*/
                                     /* | waiters (to)unblock(ed) counts
*/
                                     /* +-> Optional* Sync.LEVEL-2
*/
};                                   /* Opt*) for _timedwait and
cancellation*/

int
pthread_cond_init (pthread_cond_t * cond, const pthread_condattr_t * attr)
  int result = EAGAIN;
  pthread_cond_t cv = NULL;

  if (cond == NULL)
    {(cond
      return EINVAL;
    }

  if ((attr != NULL && *attr != NULL) &&
      ((*attr)->pshared == PTHREAD_PROCESS_SHARED))
    {
      /*
       * Creating condition variable that can be shared between
       * processes.
       */
      result = ENOSYS;

      goto FAIL0;
    }

  cv = (pthread_cond_t) calloc (1, sizeof (*cv));

  if (cv == NULL)
    {(cv
      result = ENOMEM;
      goto FAIL0;
    }

  cv->nWaitersBlocked   = 0;
  cv->nWaitersUnblocked = 0;
  cv->nWaitersToUnblock = 0;

  if (sem_init (&(cv->semBlockLock), 0, 1) != 0)
    {(sem_init
      goto FAIL0;
    }

  if (sem_init (&(cv->semBlockQueue), 0, 0) != 0)
    {(sem_init
      goto FAIL1;
    }

  if (pthread_mutex_init (&(cv->mtxUnblockLock), 0) != 0)
    {(pthread_mutex_init
      goto FAIL2;
    }


  result = 0;

  goto DONE;

  /*
   * -------------
   * Failed...
   * -------------
   */
FAIL2:
  (void) sem_destroy (&(cv->semBlockQueue));

FAIL1:
  (void) sem_destroy (&(cv->semBlockLock));

FAIL0:
DONE:
  *cond = cv;

  return (result);

}                               /* pthread_cond_init */

int
pthread_cond_destroy (pthread_cond_t * cond)
{
  int result = 0;
  pthread_cond_t cv;

  /*
   * Assuming any race condition here is harmless.
   */
  if (cond == NULL
      || *cond == NULL)
    {
      return EINVAL;
    }

  if (*cond != (pthread_cond_t) PTW32_OBJECT_AUTO_INIT)
    {(*cond
      cv = *cond;

      /*
       * Synchronize access to waiters blocked count (LEVEL-1)
       */
      if (sem_wait(&(cv->semBlockLock)) != 0)
        {(sem_wait(&(cv->semBlockLock))
          return errno;
        }

      /*
       * Synchronize access to waiters (to)unblock(ed) counts (LEVEL-2)
       */
      if ((result = pthread_mutex_lock(&(cv->mtxUnblockLock))) != 0)
        {((result
          (void) sem_post(&(cv->semBlockLock));
          return result;
        }

      /*
       * Check whether cv is still busy (still has waiters blocked)
       */
      if (cv->nWaitersBlocked - cv->nWaitersUnblocked > 0)
        {(cv->nWaitersBlocked
          (void) sem_post(&(cv->semBlockLock));
          (void) pthread_mutex_unlock(&(cv->mtxUnblockLock));
          return EBUSY;
        }

      /*
       * Now it is safe to destroy
       */
      (void) sem_destroy (&(cv->semBlockLock));
      (void) sem_destroy (&(cv->semBlockQueue));
      (void) pthread_mutex_unlock (&(cv->mtxUnblockLock));
      (void) pthread_mutex_destroy (&(cv->mtxUnblockLock));

      free(cv);
      *cond = NULL;
    }
  else
    {
      /*
       * See notes in ptw32_cond_check_need_init() above also.
       */
      EnterCriticalSection(&ptw32_cond_test_init_lock);

      /*
       * Check again.
       */
      if (*cond == (pthread_cond_t) PTW32_OBJECT_AUTO_INIT)
        {(*cond
          /*
           * This is all we need to do to destroy a statically
           * initialised cond that has not yet been used (initialised).
           * If we get to here, another thread
           * waiting to initialise this cond will get an EINVAL.
           */
          *cond = NULL;
        }
      else
        {
          /*
           * The cv has been initialised while we were waiting
           * so assume it's in use.
           */
          result = EBUSY;
        }

      LeaveCriticalSection(&ptw32_cond_test_init_lock);
    }

  return (result);
}

/*
 * Arguments for cond_wait_cleanup, since we can only pass a
 * single void * to it.
 */
typedef struct {
  pthread_mutex_t * mutexPtr;
  pthread_cond_t cv;
  int * resultPtr;
} ptw32_cond_wait_cleanup_args_t;

static void
ptw32_cond_wait_cleanup(void * args)
{
  ptw32_cond_wait_cleanup_args_t * cleanup_args =
(ptw32_cond_wait_cleanup_args_t *) args;
  pthread_cond_t cv = cleanup_args->cv;
  int * resultPtr = cleanup_args->resultPtr;
  int eLastSignal; /* enum: 1=yes 0=no -1=cancelled/timedout w/o signal(s)
*/
  int result;

  /*
   * Whether we got here as a result of signal/broadcast or because of
   * timeout on wait or thread cancellation we indicate that we are no
   * longer waiting. The waiter is responsible for adjusting waiters
   * (to)unblock(ed) counts (protected by unblock lock).
   * Unblock lock/Sync.LEVEL-2 supports _timedwait and cancellation.
   */
  if ((result = pthread_mutex_lock(&(cv->mtxUnblockLock))) != 0)
    {((result
      *resultPtr = result;
      return;
    }

  cv->nWaitersUnblocked++;

  eLastSignal = (cv->nWaitersToUnblock == 0) ?
                   -1 : (--cv->nWaitersToUnblock == 0);

  /*
   * No more LEVEL-2 access to waiters (to)unblock(ed) counts needed
   */
  if ((result = pthread_mutex_unlock(&(cv->mtxUnblockLock))) != 0)
    {((result
      *resultPtr = result;
      return;
    }

  /*
   * If last signal...
   */
  if (eLastSignal == 1)
    {(eLastSignal
     /*
      * ...it means that we have end of 'atomic' signal/broadcast
      */
      if (sem_post(&(cv->semBlockLock)) != 0)
        {(sem_post(&(cv->semBlockLock))
          *resultPtr = errno;
          return;
        }
    }
  /*
   * If not last signal and not timed out/cancelled wait w/o signal...
   */
  else if (eLastSignal == 0)
    {
     /*
      * ...it means that next waiter can go through semaphore
      */
      if (sem_post(&(cv->semBlockQueue)) != 0)
        {(sem_post(&(cv->semBlockQueue))
          *resultPtr = errno;
          return;
        }
    }

  /*
   * XSH: Upon successful return, the mutex has been locked and is owned
   * by the calling thread
   */
  if ((result = pthread_mutex_lock(cleanup_args->mutexPtr)) != 0)
    {((result
      *resultPtr = result;
    }

}                               /* ptw32_cond_wait_cleanup */

static int
ptw32_cond_timedwait (pthread_cond_t * cond,
                      pthread_mutex_t * mutex,
                      const struct timespec *abstime)
{
  int result = 0;
  pthread_cond_t cv;
  ptw32_cond_wait_cleanup_args_t cleanup_args;

  if (cond == NULL || *cond == NULL)
    {(cond
      return EINVAL;
    }

  /*
   * We do a quick check to see if we need to do more work
   * to initialise a static condition variable. We check
   * again inside the guarded section of ptw32_cond_check_need_init()
   * to avoid race conditions.
   */
  if (*cond == (pthread_cond_t) PTW32_OBJECT_AUTO_INIT)
    {(*cond
      result = ptw32_cond_check_need_init(cond);
    }

  if (result != 0 && result != EBUSY)
    {(result
      return result;
    }

  cv = *cond;

  /*
   * Synchronize access to waiters blocked count (LEVEL-1)
   */
  if (sem_wait(&(cv->semBlockLock)) != 0)
    {(sem_wait(&(cv->semBlockLock))
      return errno;
    }

  cv->nWaitersBlocked++;

  /*
   * Thats it. Counted means waiting, no more access needed
   */
  if (sem_post(&(cv->semBlockLock)) != 0)
    {(sem_post(&(cv->semBlockLock))
      return errno;
    }

  /*
   * Setup this waiter cleanup handler
   */
  cleanup_args.mutexPtr = mutex;
  cleanup_args.cv = cv;
  cleanup_args.resultPtr = &result;

  pthread_cleanup_push (ptw32_cond_wait_cleanup, (void *) &cleanup_args);

  /*
   * Now we can release 'mutex' and...
   */
  if ((result = pthread_mutex_unlock (mutex)) == 0)
    {((result

      /*
       * ...wait to be awakened by
       *              pthread_cond_signal, or
       *              pthread_cond_broadcast, or
       *              timeout, or
       *              thread cancellation
       *
       * Note:
       *
       *      ptw32_sem_timedwait is a cancellation point,
       *      hence providing the mechanism for making
       *      pthread_cond_wait a cancellation point.
       *      We use the cleanup mechanism to ensure we
       *      re-lock the mutex and adjust (to)unblock(ed) waiters
       *      counts if we are cancelled, timed out or signalled.
       */
      if (ptw32_sem_timedwait (&(cv->semBlockQueue), abstime) != 0)
        {(ptw32_sem_timedwait
          result = errno;
        }
    }

  /*
   * Always cleanup
   */
  pthread_cleanup_pop (1);


  /*
   * "result" can be modified by the cleanup handler.
   */
  return (result);

}                               /* ptw32_cond_timedwait */


static int
ptw32_cond_unblock (pthread_cond_t * cond,
                    int unblockAll)
{
  int result;
  pthread_cond_t cv;

  if (cond == NULL || *cond == NULL)
    {(cond
      return EINVAL;
    }

  cv = *cond;

  /*
   * No-op if the CV is static and hasn't been initialised yet.
   * Assuming that any race condition is harmless.
   */
  if (cv == (pthread_cond_t) PTW32_OBJECT_AUTO_INIT)
    {(cv
      return 0;
    }

  /*
   * Synchronize access to waiters blocked count (LEVEL-1)
   */
  if (sem_wait(&(cv->semBlockLock)) != 0)
    {(sem_wait(&(cv->semBlockLock))
      return errno;
    }

  /*
   * Synchronize access to waiters (to)unblock(ed) counts (LEVEL-2)
   * This sync.level supports _timedwait and cancellation
   */
  if ((result = pthread_mutex_lock(&(cv->mtxUnblockLock))) != 0)
    {((result
      return result;
    }

  /*
   * Adjust waiters blocked and unblocked counts (collect garbage)
   */
  if (cv->nWaitersUnblocked != 0)
    {(cv->nWaitersUnblocked
      cv->nWaitersBlocked  -= cv->nWaitersUnblocked;
      cv->nWaitersUnblocked = 0;
    }

  /*
   * If (after adjustment) there are still some waiters blocked counted...
   */
  if ( cv->nWaitersBlocked > 0)
    {(
      /*
       * We will unblock first waiter and leave semBlockLock/LEVEL-1 locked
       * LEVEL-1 access is left disabled until last signal/unblock
completes
       */
      cv->nWaitersToUnblock = (unblockAll) ? cv->nWaitersBlocked : 1;

      /*
       * No more LEVEL-2 access to waiters (to)unblock(ed) counts needed
       * This sync.level supports _timedwait and cancellation
       */
      if ((result = pthread_mutex_unlock(&(cv->mtxUnblockLock))) != 0)
        {((result
          return result;
        }


      /*
       * Now, with LEVEL-2 lock released let first waiter go through
semaphore
       */
      if (sem_post(&(cv->semBlockQueue)) != 0)
        {(sem_post(&(cv->semBlockQueue))
          return errno;
        }
    }
  /*
   * No waiter blocked - no more LEVEL-1 access to blocked count needed...
   */
  else if (sem_post(&(cv->semBlockLock)) != 0)
    {
      return errno;
    }
  /*
   * ...and no more LEVEL-2 access to waiters (to)unblock(ed) counts needed
too
   * This sync.level supports _timedwait and cancellation
   */
  else
    {
      result = pthread_mutex_unlock(&(cv->mtxUnblockLock));
    }

  return(result);

}                               /* ptw32_cond_unblock */

int
pthread_cond_wait (pthread_cond_t * cond,
                   pthread_mutex_t * mutex)
{
  /* The NULL abstime arg means INFINITE waiting. */
  return(ptw32_cond_timedwait(cond, mutex, NULL));
}                               /* pthread_cond_wait */


int
pthread_cond_timedwait (pthread_cond_t * cond,
                        pthread_mutex_t * mutex,
                        const struct timespec *abstime)
{
  if (abstime == NULL)
    {(abstime
      return EINVAL;
    }

  return(ptw32_cond_timedwait(cond, mutex, abstime));
}                               /* pthread_cond_timedwait */


int
pthread_cond_signal (pthread_cond_t * cond)
{
  /* The '0'(FALSE) unblockAll arg means unblock ONE waiter. */
  return(ptw32_cond_unblock(cond, 0));
}                               /* pthread_cond_signal */

int
pthread_cond_broadcast (pthread_cond_t * cond)
{
  /* The '1'(TRUE) unblockAll arg means unblock ALL waiters. */
  return(ptw32_cond_unblock(cond, 1));
}                               /* pthread_cond_broadcast */




TEREKHOV@de.ibm.com on 17.01.2001 01:00:57

Please respond to TEREKHOV@de.ibm.com

To:   pthreads-win32@sourceware.cygnus.com
cc:   schmidt@uci.edu
Subject:  win32 conditions: sem+counter+event = broadcast_deadlock +
      spur.wakeup/unfairness/incorrectness ??







Hi,

Problem 1: broadcast_deadlock

It seems that current implementation does not provide "atomic"
broadcasts. That may lead to "nested" broadcasts... and it seems
that nested case is not handled correctly -> producing a broadcast
DEADLOCK as a result.

Scenario:

N (>1) waiting threads W1..N are blocked (in _wait) on condition's
semaphore.

Thread B1 calls pthread_cond_broadcast, which results in "releasing" N
W threads via incrementing semaphore counter by N (stored in
cv->waiters) BUT cv->waiters counter does not change!! The caller
thread B1 remains blocked on cv->waitersDone event (auto-reset!!) BUT
condition is not protected from starting another broadcast (when called
on another thread) while still waiting for the "old" broadcast to
complete on thread B1.

M (>=0, <N) W threads are fast enough to go thru their _wait call and
decrement cv->waiters counter.

L (N-M) "late" waiter W threads are a) still blocked/not returned from
their semaphore wait call or b) were preempted after sem_wait but before
lock( &cv->waitersLock ) or c) are blocked on cv->waitersLock.

cv->waiters is still > 0 (= L).

Another thread B2 (or some W thread from M group) calls
pthread_cond_broadcast and gains access to counter... neither a) nor b)
prevent thread B2 in pthread_cond_broadcast from gaining access to
counter and starting another broadcast ( for c) - it depends on
cv->waitersLock scheduling rules: FIFO=OK, PRTY=PROBLEM,... )

That call to pthread_cond_broadcast (on thread B2) will result in
incrementing semaphore by cv->waiters (=L) which is INCORRECT (all
W1..N were in fact already released by thread B1) and waiting on
_auto-reset_ event cv->waitersDone which is DEADLY WRONG (produces a
deadlock)...

All late W1..L threads now have a chance to complete their _wait call.
Last W_L thread sets an auto-reselt event cv->waitersDone which will
release either B1 or B2 leaving one of B threads in a deadlock.

Problem 2: spur.wakeup/unfairness/incorrectness

It seems that:

a) because of the same problem with counter which does not reflect the
actual number of NOT RELEASED waiters, the signal call may increment
a semaphore counter w/o having a waiter blocked on it. That will result
in (best case) spurious wake ups - performance degradation due to
unnecessary context switches and predicate re-checks and (in worth case)
unfairness/incorrectness problem - see b)

b) neither signal nor broadcast prevent other threads - "new waiters"
(and in the case of signal, the caller thread as well) from going into
_wait and overtaking "old" waiters (already released but still not returned
from sem_wait on condition's semaphore). Win semaphore just [API DOC]:
"Maintains a count between zero and some maximum value, limiting the number
of threads that are simultaneously accessing a shared resource." Calling
ReleaseSemaphore does not imply (at least not documented) that on return
from ReleaseSemaphore all waiters will in fact become released (returned
from their Wait... call) and/or that new waiters calling Wait... afterwards
will become less importance. It is NOT documented to be an atomic release
of
waiters... And even if it would be there is still a problem with a thread
being preempted after Wait on semaphore and before Wait on cv->waitersLock
and scheduling rules for cv->waitersLock itself
(??WaitForMultipleObjects??)
That may result in unfairness/incorrectness problem as described
for SetEvent impl. in "Strategies for Implementing POSIX Condition
Variables
on Win32": http://www.cs.wustl.edu/~schmidt/win32-cv-1.html

Unfairness -- The semantics of the POSIX pthread_cond_broadcast function is
to wake up all threads currently blocked in wait calls on the condition
variable. The awakened threads then compete for the external_mutex. To
ensure
fairness, all of these threads should be released from their
pthread_cond_wait calls and allowed to recheck their condition expressions
before other threads can successfully complete a wait on the condition
variable.

Unfortunately, the SetEvent implementation above does not guarantee that
all
threads sleeping on the condition variable when cond_broadcast is called
will
acquire the external_mutex and check their condition expressions. Although
the Pthreads specification does not mandate this degree of fairness, the
lack of fairness can cause starvation.

To illustrate the unfairness problem, imagine there are 2 threads, C1 and
C2,
that are blocked in pthread_cond_wait on condition variable not_empty_ that
is guarding a thread-safe message queue. Another thread, P1 then places two
messages onto the queue and calls pthread_cond_broadcast. If C1 returns
from
pthread_cond_wait, dequeues and processes the message, and immediately
waits
again then it and only it may end up acquiring both messages. Thus, C2 will
never get a chance to dequeue a message and run.

The following illustrates the sequence of events:

1.   Thread C1 attempts to dequeue and waits on CV non_empty_
2.   Thread C2 attempts to dequeue and waits on CV non_empty_
3.   Thread P1 enqueues 2 messages and broadcasts to CV not_empty_
4.   Thread P1 exits
5.   Thread C1 wakes up from CV not_empty_, dequeues a message and runs
6.   Thread C1 waits again on CV not_empty_, immediately dequeues the 2nd
        message and runs
7.   Thread C1 exits
8.   Thread C2 is the only thread left and blocks forever since
        not_empty_ will never be signaled

Depending on the algorithm being implemented, this lack of fairness may
yield
concurrent programs that have subtle bugs. Of course, application
developers
should not rely on the fairness semantics of pthread_cond_broadcast.
However,
there are many cases where fair implementations of condition variables can
simplify application code.

Incorrectness -- A variation on the unfairness problem described above
occurs
when a third consumer thread, C3, is allowed to slip through even though it
was not waiting on condition variable not_empty_ when a broadcast occurred.

To illustrate this, we will use the same scenario as above: 2 threads, C1
and
C2, are blocked dequeuing messages from the message queue. Another thread,
P1
then places two messages onto the queue and calls pthread_cond_broadcast.
C1
returns from pthread_cond_wait, dequeues and processes the message. At this
time, C3 acquires the external_mutex, calls pthread_cond_wait and waits on
the events in WaitForMultipleObjects. Since C2 has not had a chance to run
yet, the BROADCAST event is still signaled. C3 then returns from
WaitForMultipleObjects, and dequeues and processes the message in the
queue.
Thus, C2 will never get a chance to dequeue a message and run.

The following illustrates the sequence of events:

1.   Thread C1 attempts to dequeue and waits on CV non_empty_
2.   Thread C2 attempts to dequeue and waits on CV non_empty_
3.   Thread P1 enqueues 2 messages and broadcasts to CV not_empty_
4.   Thread P1 exits
5.   Thread C1 wakes up from CV not_empty_, dequeues a message and runs
6.   Thread C1 exits
7.   Thread C3 waits on CV not_empty_, immediately dequeues the 2nd
        message and runs
8.   Thread C3 exits
9.   Thread C2 is the only thread left and blocks forever since
        not_empty_ will never be signaled

In the above case, a thread that was not waiting on the condition variable
when a broadcast occurred was allowed to proceed. This leads to incorrect
semantics for a condition variable.


COMMENTS???

regards,
alexander.

-----------------------------------------------------------------------------

Subject: RE: FYI/comp.programming.threads/Re: pthread_cond_*
     implementation questions
Date: Wed, 21 Feb 2001 11:54:47 +0100
From: TEREKHOV@de.ibm.com
To: lthomas@arbitrade.com
CC: rpj@ise.canberra.edu.au, Thomas Pfaff <tpfaff@gmx.net>,
     Nanbor Wang <nanbor@cs.wustl.edu>

Hi Louis,

generation number 8..

had some time to revisit timeouts/spurious wakeup problem..
found some bugs (in 7.b/c/d) and something to improve
(7a - using IPC semaphores but it should speedup Win32
version as well).

regards,
alexander.

---------- Algorithm 8a / IMPL_SEM,UNBLOCK_STRATEGY == UNBLOCK_ALL ------
given:
semBlockLock - bin.semaphore
semBlockQueue - semaphore
mtxExternal - mutex or CS
mtxUnblockLock - mutex or CS
nWaitersGone - int
nWaitersBlocked - int
nWaitersToUnblock - int

wait( timeout ) {

  [auto: register int result          ]     // error checking omitted
  [auto: register int nSignalsWasLeft ]
  [auto: register int nWaitersWasGone ]

  sem_wait( semBlockLock );
  nWaitersBlocked++;
  sem_post( semBlockLock );

  unlock( mtxExternal );
  bTimedOut = sem_wait( semBlockQueue,timeout );

  lock( mtxUnblockLock );
  if ( 0 != (nSignalsWasLeft = nWaitersToUnblock) ) {
    if ( bTimeout ) {                       // timeout (or canceled)
      if ( 0 != nWaitersBlocked ) {
        nWaitersBlocked--;
      }
      else {
        nWaitersGone++;                     // count spurious wakeups
      }
    }
    if ( 0 == --nWaitersToUnblock ) {
      if ( 0 != nWaitersBlocked ) {
        sem_post( semBlockLock );           // open the gate
        nSignalsWasLeft = 0;                // do not open the gate below
again
      }
      else if ( 0 != (nWaitersWasGone = nWaitersGone) ) {
        nWaitersGone = 0;
      }
    }
  }
  else if ( INT_MAX/2 == ++nWaitersGone ) { // timeout/canceled or spurious
semaphore :-)
    sem_wait( semBlockLock );
    nWaitersBlocked -= nWaitersGone;        // something is going on here -
test of timeouts? :-)
    sem_post( semBlockLock );
    nWaitersGone = 0;
  }
  unlock( mtxUnblockLock );

  if ( 1 == nSignalsWasLeft ) {
    if ( 0 != nWaitersWasGone ) {
      // sem_adjust( -nWaitersWasGone );
      while ( nWaitersWasGone-- ) {
        sem_wait( semBlockLock );          // better now than spurious
later
      }
    }
    sem_post( semBlockLock );              // open the gate
  }

  lock( mtxExternal );

  return ( bTimedOut ) ? ETIMEOUT : 0;
}

signal(bAll) {

  [auto: register int result         ]
  [auto: register int nSignalsToIssue]

  lock( mtxUnblockLock );

  if ( 0 != nWaitersToUnblock ) { // the gate is closed!!!
    if ( 0 == nWaitersBlocked ) { // NO-OP
      return unlock( mtxUnblockLock );
    }
    if (bAll) {
      nWaitersToUnblock += nSignalsToIssue=nWaitersBlocked;
      nWaitersBlocked = 0;
    }
    else {
      nSignalsToIssue = 1;
      nWaitersToUnblock++;
      nWaitersBlocked--;
    }
  }
  else if ( nWaitersBlocked > nWaitersGone ) { // HARMLESS RACE CONDITION!
    sem_wait( semBlockLock ); // close the gate
    if ( 0 != nWaitersGone ) {
      nWaitersBlocked -= nWaitersGone;
      nWaitersGone = 0;
    }
    if (bAll) {
      nSignalsToIssue = nWaitersToUnblock = nWaitersBlocked;
      nWaitersBlocked = 0;
    }
    else {
      nSignalsToIssue = nWaitersToUnblock = 1;
      nWaitersBlocked--;
    }
  }
  else { // NO-OP
    return unlock( mtxUnblockLock );
  }

  unlock( mtxUnblockLock );
  sem_post( semBlockQueue,nSignalsToIssue );
  return result;
}

---------- Algorithm 8b / IMPL_SEM,UNBLOCK_STRATEGY == UNBLOCK_ONEBYONE
------
given:
semBlockLock - bin.semaphore
semBlockQueue - bin.semaphore
mtxExternal - mutex or CS
mtxUnblockLock - mutex or CS
nWaitersGone - int
nWaitersBlocked - int
nWaitersToUnblock - int

wait( timeout ) {

  [auto: register int result          ]     // error checking omitted
  [auto: register int nWaitersWasGone ]
  [auto: register int nSignalsWasLeft ]

  sem_wait( semBlockLock );
  nWaitersBlocked++;
  sem_post( semBlockLock );

  unlock( mtxExternal );
  bTimedOut = sem_wait( semBlockQueue,timeout );

  lock( mtxUnblockLock );
  if ( 0 != (nSignalsWasLeft = nWaitersToUnblock) ) {
    if ( bTimeout ) {                       // timeout (or canceled)
      if ( 0 != nWaitersBlocked ) {
        nWaitersBlocked--;
        nSignalsWasLeft = 0;                // do not unblock next waiter
below (already unblocked)
      }
      else {
        nWaitersGone = 1;                   // spurious wakeup pending!!
      }
    }
    if ( 0 == --nWaitersToUnblock &&
      if ( 0 != nWaitersBlocked ) {
        sem_post( semBlockLock );           // open the gate
        nSignalsWasLeft = 0;                // do not open the gate below
again
      }
      else if ( 0 != (nWaitersWasGone = nWaitersGone) ) {
        nWaitersGone = 0;
      }
    }
  }
  else if ( INT_MAX/2 == ++nWaitersGone ) { // timeout/canceled or spurious
semaphore :-)
    sem_wait( semBlockLock );
    nWaitersBlocked -= nWaitersGone;        // something is going on here -
test of timeouts? :-)
    sem_post( semBlockLock );
    nWaitersGone = 0;
  }
  unlock( mtxUnblockLock );

  if ( 1 == nSignalsWasLeft ) {
    if ( 0 != nWaitersWasGone ) {
      // sem_adjust( -1 );
      sem_wait( semBlockQueue );           // better now than spurious
later
    }
    sem_post( semBlockLock );              // open the gate
  }
  else if ( 0 != nSignalsWasLeft ) {
    sem_post( semBlockQueue );             // unblock next waiter
  }

  lock( mtxExternal );

  return ( bTimedOut ) ? ETIMEOUT : 0;
}

signal(bAll) {

  [auto: register int result ]

  lock( mtxUnblockLock );

  if ( 0 != nWaitersToUnblock ) { // the gate is closed!!!
    if ( 0 == nWaitersBlocked ) { // NO-OP
      return unlock( mtxUnblockLock );
    }
    if (bAll) {
      nWaitersToUnblock += nWaitersBlocked;
      nWaitersBlocked = 0;
    }
    else {
      nWaitersToUnblock++;
      nWaitersBlocked--;
    }
    unlock( mtxUnblockLock );
  }
  else if ( nWaitersBlocked > nWaitersGone ) { // HARMLESS RACE CONDITION!
    sem_wait( semBlockLock ); // close the gate
    if ( 0 != nWaitersGone ) {
      nWaitersBlocked -= nWaitersGone;
      nWaitersGone = 0;
    }
    if (bAll) {
      nWaitersToUnblock = nWaitersBlocked;
      nWaitersBlocked = 0;
    }
    else {
      nWaitersToUnblock = 1;
      nWaitersBlocked--;
    }
    unlock( mtxUnblockLock );
    sem_post( semBlockQueue );
  }
  else { // NO-OP
    unlock( mtxUnblockLock );
  }

  return result;
}

---------- Algorithm 8c / IMPL_EVENT,UNBLOCK_STRATEGY == UNBLOCK_ONEBYONE
---------
given:
hevBlockLock - auto-reset event
hevBlockQueue - auto-reset event
mtxExternal - mutex or CS
mtxUnblockLock - mutex or CS
nWaitersGone - int
nWaitersBlocked - int
nWaitersToUnblock - int

wait( timeout ) {

  [auto: register int result          ]     // error checking omitted
  [auto: register int nSignalsWasLeft ]
  [auto: register int nWaitersWasGone ]

  wait( hevBlockLock,INFINITE );
  nWaitersBlocked++;
  set_event( hevBlockLock );

  unlock( mtxExternal );
  bTimedOut = wait( hevBlockQueue,timeout );

  lock( mtxUnblockLock );
  if ( 0 != (SignalsWasLeft = nWaitersToUnblock) ) {
    if ( bTimeout ) {                       // timeout (or canceled)
      if ( 0 != nWaitersBlocked ) {
        nWaitersBlocked--;
        nSignalsWasLeft = 0;                // do not unblock next waiter
below (already unblocked)
      }
      else {
        nWaitersGone = 1;                   // spurious wakeup pending!!
      }
    }
    if ( 0 == --nWaitersToUnblock )
      if ( 0 != nWaitersBlocked ) {
        set_event( hevBlockLock );          // open the gate
        nSignalsWasLeft = 0;                // do not open the gate below
again
      }
      else if ( 0 != (nWaitersWasGone = nWaitersGone) ) {
        nWaitersGone = 0;
      }
    }
  }
  else if ( INT_MAX/2 == ++nWaitersGone ) { // timeout/canceled or spurious
event :-)
    wait( hevBlockLock,INFINITE );
    nWaitersBlocked -= nWaitersGone;        // something is going on here -
test of timeouts? :-)
    set_event( hevBlockLock );
    nWaitersGone = 0;
  }
  unlock( mtxUnblockLock );

  if ( 1 == nSignalsWasLeft ) {
    if ( 0 != nWaitersWasGone ) {
      reset_event( hevBlockQueue );         // better now than spurious
later
    }
    set_event( hevBlockLock );              // open the gate
  }
  else if ( 0 != nSignalsWasLeft ) {
    set_event( hevBlockQueue );             // unblock next waiter
  }

  lock( mtxExternal );

  return ( bTimedOut ) ? ETIMEOUT : 0;
}

signal(bAll) {

  [auto: register int result ]

  lock( mtxUnblockLock );

  if ( 0 != nWaitersToUnblock ) { // the gate is closed!!!
    if ( 0 == nWaitersBlocked ) { // NO-OP
      return unlock( mtxUnblockLock );
    }
    if (bAll) {
      nWaitersToUnblock += nWaitersBlocked;
      nWaitersBlocked = 0;
    }
    else {
      nWaitersToUnblock++;
      nWaitersBlocked--;
    }
    unlock( mtxUnblockLock );
  }
  else if ( nWaitersBlocked > nWaitersGone ) { // HARMLESS RACE CONDITION!
    wait( hevBlockLock,INFINITE ); // close the gate
    if ( 0 != nWaitersGone ) {
      nWaitersBlocked -= nWaitersGone;
      nWaitersGone = 0;
    }
    if (bAll) {
      nWaitersToUnblock = nWaitersBlocked;
      nWaitersBlocked = 0;
    }
    else {
      nWaitersToUnblock = 1;
      nWaitersBlocked--;
    }
    unlock( mtxUnblockLock );
    set_event( hevBlockQueue );
  }
  else { // NO-OP
    unlock( mtxUnblockLock );
  }

  return result;
}

---------- Algorithm 8d / IMPL_EVENT,UNBLOCK_STRATEGY == UNBLOCK_ALL ------
given:
hevBlockLock - auto-reset event
hevBlockQueueS - auto-reset event  // for signals
hevBlockQueueB - manual-reset even // for broadcasts
mtxExternal - mutex or CS
mtxUnblockLock - mutex or CS
eBroadcast - int                   // 0: no broadcast, 1: broadcast, 2:
broadcast after signal(s)
nWaitersGone - int
nWaitersBlocked - int
nWaitersToUnblock - int

wait( timeout ) {

  [auto: register int result          ]     // error checking omitted
  [auto: register int eWasBroadcast   ]
  [auto: register int nSignalsWasLeft ]
  [auto: register int nWaitersWasGone ]

  wait( hevBlockLock,INFINITE );
  nWaitersBlocked++;
  set_event( hevBlockLock );

  unlock( mtxExternal );
  bTimedOut = waitformultiple( hevBlockQueueS,hevBlockQueueB,timeout,ONE );

  lock( mtxUnblockLock );
  if ( 0 != (SignalsWasLeft = nWaitersToUnblock) ) {
    if ( bTimeout ) {                       // timeout (or canceled)
      if ( 0 != nWaitersBlocked ) {
        nWaitersBlocked--;
        nSignalsWasLeft = 0;                // do not unblock next waiter
below (already unblocked)
      }
      else if ( 1 != eBroadcast ) {
        nWaitersGone = 1;
      }
    }
    if ( 0 == --nWaitersToUnblock ) {
      if ( 0 != nWaitersBlocked ) {
        set_event( hevBlockLock );           // open the gate
        nSignalsWasLeft = 0;                 // do not open the gate below
again
      }
      else {
        if ( 0 != (eWasBroadcast = eBroadcast) ) {
          eBroadcast = 0;
        }
        if ( 0 != (nWaitersWasGone = nWaitersGone ) {
          nWaitersGone = 0;
        }
      }
    }
    else if ( 0 != eBroadcast ) {
      nSignalsWasLeft = 0;                  // do not unblock next waiter
below (already unblocked)
    }
  }
  else if ( INT_MAX/2 == ++nWaitersGone ) { // timeout/canceled or spurious
event :-)
    wait( hevBlockLock,INFINITE );
    nWaitersBlocked -= nWaitersGone;        // something is going on here -
test of timeouts? :-)
    set_event( hevBlockLock );
    nWaitersGone = 0;
  }
  unlock( mtxUnblockLock );

  if ( 1 == nSignalsWasLeft ) {
    if ( 0 != eWasBroadcast ) {
      reset_event( hevBlockQueueB );
    }
    if ( 0 != nWaitersWasGone ) {
      reset_event( hevBlockQueueS );        // better now than spurious
later
    }
    set_event( hevBlockLock );              // open the gate
  }
  else if ( 0 != nSignalsWasLeft ) {
    set_event( hevBlockQueueS );            // unblock next waiter
  }

  lock( mtxExternal );

  return ( bTimedOut ) ? ETIMEOUT : 0;
}

signal(bAll) {

  [auto: register int    result        ]
  [auto: register HANDLE hevBlockQueue ]

  lock( mtxUnblockLock );

  if ( 0 != nWaitersToUnblock ) { // the gate is closed!!!
    if ( 0 == nWaitersBlocked ) { // NO-OP
      return unlock( mtxUnblockLock );
    }
    if (bAll) {
      nWaitersToUnblock += nWaitersBlocked;
      nWaitersBlocked = 0;
      eBroadcast = 2;
      hevBlockQueue = hevBlockQueueB;
    }
    else {
      nWaitersToUnblock++;
      nWaitersBlocked--;
      return unlock( mtxUnblockLock );
    }
  }
  else if ( nWaitersBlocked > nWaitersGone ) { // HARMLESS RACE CONDITION!
    wait( hevBlockLock,INFINITE ); // close the gate
    if ( 0 != nWaitersGone ) {
      nWaitersBlocked -= nWaitersGone;
      nWaitersGone = 0;
    }
    if (bAll) {
      nWaitersToUnblock = nWaitersBlocked;
      nWaitersBlocked = 0;
      eBroadcast = 1;
      hevBlockQueue = hevBlockQueueB;
    }
    else {
      nWaitersToUnblock = 1;
      nWaitersBlocked--;
      hevBlockQueue = hevBlockQueueS;
    }
  }
  else { // NO-OP
    return unlock( mtxUnblockLock );
  }

  unlock( mtxUnblockLock );
  set_event( hevBlockQueue );
  return result;
}
---------------------- Forwarded by Alexander Terekhov/Germany/IBM on
02/21/2001 09:13 AM ---------------------------

Alexander Terekhov
02/20/2001 04:33 PM

To:   Louis Thomas <lthomas@arbitrade.com>
cc:

From: Alexander Terekhov/Germany/IBM@IBMDE
Subject:  RE: FYI/comp.programming.threads/Re: pthread_cond_* implementatio
      n questions
Importance:    Normal

>Sorry, gotta take a break and work on something else for a while.
>Real work
>calls, unfortunately. I'll get back to you in two or three days.

ok. no problem. here is some more stuff for pauses you might have
in between :)

---------- Algorithm 7d / IMPL_EVENT,UNBLOCK_STRATEGY == UNBLOCK_ALL ------
given:
hevBlockLock - auto-reset event
hevBlockQueueS - auto-reset event  // for signals
hevBlockQueueB - manual-reset even // for broadcasts
mtxExternal - mutex or CS
mtxUnblockLock - mutex or CS
bBroadcast - int
nWaitersGone - int
nWaitersBlocked - int
nWaitersToUnblock - int

wait( timeout ) {

  [auto: register int result          ]     // error checking omitted
  [auto: register int bWasBroadcast   ]
  [auto: register int nSignalsWasLeft ]

  wait( hevBlockLock,INFINITE );
  nWaitersBlocked++;
  set_event( hevBlockLock );

  unlock( mtxExternal );
  bTimedOut = waitformultiple( hevBlockQueueS,hevBlockQueueB,timeout,ONE );

  lock( mtxUnblockLock );
  if ( 0 != (SignalsWasLeft = nWaitersToUnblock) ) {
    if ( bTimeout ) {                       // timeout (or canceled)
      if ( 0 != nWaitersBlocked ) {
        nWaitersBlocked--;
        nSignalsWasLeft = 0;                // do not unblock next waiter
below (already unblocked)
      }
      else if ( !bBroadcast ) {
        wait( hevBlockQueueS,INFINITE );    // better now than spurious
later
      }
    }
    if ( 0 == --nWaitersToUnblock ) {
      if ( 0 != nWaitersBlocked ) {
        if ( bBroadcast ) {
          reset_event( hevBlockQueueB );
          bBroadcast = false;
        }
        set_event( hevBlockLock );           // open the gate
        nSignalsWasLeft = 0;                 // do not open the gate below
again
      }
      else if ( false != (bWasBroadcast = bBroadcast) ) {
        bBroadcast = false;
      }
    }
    else {
      bWasBroadcast = bBroadcast;
    }
  }
  else if ( INT_MAX/2 == ++nWaitersGone ) { // timeout/canceled or spurious
event :-)
    wait( hevBlockLock,INFINITE );
    nWaitersBlocked -= nWaitersGone;        // something is going on here -
test of timeouts? :-)
    set_event( hevBlockLock );
    nWaitersGone = 0;
  }
  unlock( mtxUnblockLock );

  if ( 1 == nSignalsWasLeft ) {
    if ( bWasBroadcast ) {
      reset_event( hevBlockQueueB );
    }
    set_event( hevBlockLock );              // open the gate
  }
  else if ( 0 != nSignalsWasLeft && !bWasBroadcast ) {
    set_event( hevBlockQueueS );            // unblock next waiter
  }

  lock( mtxExternal );

  return ( bTimedOut ) ? ETIMEOUT : 0;
}

signal(bAll) {

  [auto: register int    result        ]
  [auto: register HANDLE hevBlockQueue ]

  lock( mtxUnblockLock );

  if ( 0 != nWaitersToUnblock ) { // the gate is closed!!!
    if ( 0 == nWaitersBlocked ) { // NO-OP
      return unlock( mtxUnblockLock );
    }
    if (bAll) {
      nWaitersToUnblock += nWaitersBlocked;
      nWaitersBlocked = 0;
      bBroadcast = true;
      hevBlockQueue = hevBlockQueueB;
    }
    else {
      nWaitersToUnblock++;
      nWaitersBlocked--;
      return unlock( mtxUnblockLock );
    }
  }
  else if ( nWaitersBlocked > nWaitersGone ) { // HARMLESS RACE CONDITION!
    wait( hevBlockLock,INFINITE ); // close the gate
    if ( 0 != nWaitersGone ) {
      nWaitersBlocked -= nWaitersGone;
      nWaitersGone = 0;
    }
    if (bAll) {
      nWaitersToUnblock = nWaitersBlocked;
      nWaitersBlocked = 0;
      bBroadcast = true;
      hevBlockQueue = hevBlockQueueB;
    }
    else {
      nWaitersToUnblock = 1;
      nWaitersBlocked--;
      hevBlockQueue = hevBlockQueueS;
    }
  }
  else { // NO-OP
    return unlock( mtxUnblockLock );
  }

  unlock( mtxUnblockLock );
  set_event( hevBlockQueue );
  return result;
}


----------------------------------------------------------------------------

Subject: RE: FYI/comp.programming.threads/Re: pthread_cond_* implementatio
     n questions
Date: Mon, 26 Feb 2001 22:20:12 -0600
From: Louis Thomas <lthomas@arbitrade.com>
To: "'TEREKHOV@de.ibm.com'" <TEREKHOV@de.ibm.com>
CC: rpj@ise.canberra.edu.au, Thomas Pfaff <tpfaff@gmx.net>,
     Nanbor Wang
     <nanbor@cs.wustl.edu>

Sorry all. Busy week.

> this insures the fairness
> which POSIX does not (e.g. two subsequent broadcasts - the gate does
insure
> that first wave waiters will start the race for the mutex before waiters
> from the second wave - Linux pthreads process/unblock both waves
> concurrently...)

I'm not sure how we are any more fair about this than Linux. We certainly
don't guarantee that the threads released by the first broadcast will get
the external mutex before the threads of the second wave. In fact, it is
possible that those threads will never get the external mutex if there is
enough contention for it.

> e.g. i was thinking about implementation with a pool of
> N semaphores/counters [...]

I considered that too. The problem is as you mentioned in a). You really
need to assign threads to semaphores once you know how you want to wake them
up, not when they first begin waiting which is the only time you can assign
them.

> well, i am not quite sure that i've fully understood your scenario,

Hmm. Well, it think it's an important example, so I'll try again. First, we
have thread A which we KNOW is waiting on a condition. As soon as it becomes
unblocked for any reason, we will know because it will set a flag. Since the
flag is not set, we are 100% confident that thread A is waiting on the
condition. We have another thread, thread B, which has acquired the mutex
and is about to wait on the condition. Thus it is pretty clear that at any
point, either just A is waiting, or A and B are waiting. Now thread C comes
along. C is about to do a broadcast on the condition. A broadcast is
guaranteed to unblock all threads currently waiting on a condition, right?
Again, we said that either just A is waiting, or A and B are both waiting.
So, when C does its broadcast, depending upon whether B has started waiting
or not, thread C will unblock A or unblock A and B. Either way, C must
unblock A, right?

Now, you said anything that happens is correct so long as a) "a signal is
not lost between unlocking the mutex and waiting on the condition" and b) "a
thread must not steal a signal it sent", correct? Requirement b) is easy to
satisfy: in this scenario, thread C will never wait on the condition, so it
won't steal any signals.  Requirement a) is not hard either. The only way we
could fail to meet requirement a) in this scenario is if thread B was
started waiting but didn't wake up because a signal was lost. This will not
happen.

Now, here is what happens. Assume thread C beats thread B. Thread C looks to
see how many threads are waiting on the condition. Thread C sees just one
thread, thread A, waiting. It does a broadcast waking up just one thread
because just one thread is waiting. Next, before A can become unblocked,
thread B begins waiting. Now there are two threads waiting, but only one
will be unblocked. Suppose B wins. B will become unblocked. A will not
become unblocked, because C only unblocked one thread (sema_post cond, 1).
So at the end, B finishes and A remains blocked.

We have met both of your requirements, so by your rules, this is an
acceptable outcome. However, I think that the spec says this is an
unacceptable outcome! We know for certain that A was waiting and that C did
a broadcast, but A did not become unblocked! Yet, the spec says that a
broadcast wakes up all waiting threads. This did not happen. Do you agree
that this shows your rules are not strict enough?

> and what about N2? :) this one does allow almost everything.

Don't get me started about rule #2. I'll NEVER advocate an algorithm that
uses rule 2 as an excuse to suck!

> but it is done (decrement)under mutex protection - this is not a subject
> of a race condition.

You are correct. My mistake.

> i would remove "_bTimedOut=false".. after all, it was a real timeout..

I disagree. A thread that can't successfully retract its waiter status can't
really have timed out. If a thread can't return without executing extra code
to deal with the fact that someone tried to unblock it, I think it is a poor
idea to pretend we
didn't realize someone was trying to signal us. After all, a signal is more
important than a time out.

> when nSignaled != 0, it is possible to update nWaiters (--) and do not
> touch nGone

I realize this, but I was thinking that writing it the other ways saves
another if statement.

> adjust only if nGone != 0 and save one cache memory write - probably much
slower than 'if'

Hmm. You are probably right.

> well, in a strange (e.g. timeout test) program you may (theoretically)
> have an overflow of nWaiters/nGone counters (with waiters repeatedly
timing
> out and no signals at all).

Also true. Not only that, but you also have the possibility that one could
overflow the number of waiters as well! However, considering the limit you
have chosen for nWaitersGone, I suppose it is unlikely that anyone would be
able to get INT_MAX/2 threads waiting on a single condition. :)

Analysis of 8a:

It looks correct to me.

What are IPC semaphores?

In the line where you state, "else if ( nWaitersBlocked > nWaitersGone ) {
// HARMLESS RACE CONDITION!" there is no race condition for nWaitersGone
because nWaitersGone is never modified without holding mtxUnblockLock. You
are correct that there is a harmless race on nWaitersBlocked, which can
increase and make the condition become true just after we check it. If this
happens, we interpret it as the wait starting after the signal.

I like your optimization of this. You could improve Alg. 6 as follows:
---------- Algorithm 6b ----------
signal(bAll) {
  _nSig=0
  lock counters
  // this is safe because nWaiting can only be decremented by a thread that
  // owns counters and nGone can only be changed by a thread that owns
counters.
  if (nWaiting>nGone) {
    if (0==nSignaled) {
      sema_wait gate // close gate if not already closed
    }
    if (nGone>0) {
      nWaiting-=nGone
      nGone=0
    }
    _nSig=bAll?nWaiting:1
    nSignaled+=_nSig
    nWaiting-=_nSig
  }
  unlock counters
  if (0!=_nSig) {
    sema_post queue, _nSig
  }
}
---------- ---------- ----------
I guess this wouldn't apply to Alg 8a because nWaitersGone changes meanings
depending upon whether the gate is open or closed.

In the loop "while ( nWaitersWasGone-- ) {" you do a sema_wait on
semBlockLock. Perhaps waiting on semBlockQueue would be a better idea.

What have you gained by making the last thread to be signaled do the waits
for all the timed out threads, besides added complexity? It took me a long
time to figure out what your objective was with this, to realize you were
using nWaitersGone to mean two different things, and to verify that you
hadn't introduced any bug by doing this. Even now I'm not 100% sure.

What has all this playing about with nWaitersGone really gained us besides a
lot of complexity (it is much harder to verify that this solution is
correct), execution overhead (we now have a lot more if statements to
evaluate), and space overhead (more space for the extra code, and another
integer in our data)? We did manage to save a lock/unlock pair in an
uncommon case (when a time out occurs) at the above mentioned expenses in
the common cases.

As for 8b, c, and d, they look ok though I haven't studied them thoroughly.
What would you use them for?

    Later,
        -Louis! :)

-----------------------------------------------------------------------------

Subject: RE: FYI/comp.programming.threads/Re: pthread_cond_* implementatio
     n questions
Date: Tue, 27 Feb 2001 15:51:28 +0100
From: TEREKHOV@de.ibm.com
To: Louis Thomas <lthomas@arbitrade.com>
CC: rpj@ise.canberra.edu.au, Thomas Pfaff <tpfaff@gmx.net>,
     Nanbor Wang <nanbor@cs.wustl.edu>

Hi Louis,

>> that first wave waiters will start the race for the mutex before waiters
>> from the second wave - Linux pthreads process/unblock both waves
>> concurrently...)
>
>I'm not sure how we are any more fair about this than Linux. We certainly
>don't guarantee that the threads released by the first broadcast will get
>the external mutex before the threads of the second wave. In fact, it is
>possible that those threads will never get the external mutex if there is
>enough contention for it.

correct. but gate is nevertheless more fair than Linux because of the
barrier it establishes between two races (1st and 2nd wave waiters) for
the mutex which under 'normal' circumstances (e.g. all threads of equal
priorities,..) will 'probably' result in fair behaviour with respect to
mutex ownership.

>> well, i am not quite sure that i've fully understood your scenario,
>
>Hmm. Well, it think it's an important example, so I'll try again. ...

ok. now i seem to understand this example. well, now it seems to me
that the only meaningful rule is just:

a) "a signal is not lost between unlocking the mutex and waiting on the
condition"

and that the rule

b) "a thread must not steal a signal it sent"

is not needed at all because a thread which violates b) also violates a).

i'll try to explain..

i think that the most important thing is how POSIX defines waiter's
visibility:

"if another thread is able to acquire the mutex after the about-to-block
thread
has released it, then a subsequent call to pthread_cond_signal() or
pthread_cond_broadcast() in that thread behaves as if it were issued after
the about-to-block thread has blocked. "

my understanding is the following:

1) there is no guarantees whatsoever with respect to whether
signal/broadcast
will actually unblock any 'waiter' if it is done w/o acquiring the mutex
first
(note that a thread may release it before signal/broadcast - it does not
matter).

2) it is guaranteed that waiters become 'visible' - eligible for unblock as
soon
as signalling thread acquires the mutex (but not before!!)

so..

>So, when C does its broadcast, depending upon whether B has started
waiting
>or not, thread C will unblock A or unblock A and B. Either way, C must
>unblock A, right?

right. but only if C did acquire the mutex prior to broadcast (it may
release it before broadcast as well).

implementation will violate waiters visibility rule (signal will become
lost)
if C will not unblock A.

>Now, here is what happens. Assume thread C beats thread B. Thread C looks
to
>see how many threads are waiting on the condition. Thread C sees just one
>thread, thread A, waiting. It does a broadcast waking up just one thread
>because just one thread is waiting. Next, before A can become unblocked,
>thread B begins waiting. Now there are two threads waiting, but only one
>will be unblocked. Suppose B wins. B will become unblocked. A will not
>become unblocked, because C only unblocked one thread (sema_post cond, 1).
>So at the end, B finishes and A remains blocked.

thread C did acquire the mutex ("Thread C sees just one thread, thread A,
waiting"). beginning from that moment it is guaranteed that subsequent
broadcast will unblock A. Otherwise we will have a lost signal with respect
to A. I do think that it does not matter whether the signal was physically
(completely) lost or was just stolen by another thread (B) - in both cases
it was simply lost with respect to A.

>..Do you agree that this shows your rules are not strict enough?

probably the opposite.. :-) i think that it shows that the only meaningful
rule is

a) "a signal is not lost between unlocking the mutex and waiting on the
condition"

with clarification of waiters visibility as defined by POSIX above.

>> i would remove "_bTimedOut=false".. after all, it was a real timeout..
>
>I disagree. A thread that can't successfully retract its waiter status
can't
>really have timed out. If a thread can't return without executing extra
code
>to deal with the fact that someone tried to unblock it, I think it is a
poor
>idea to pretend we
>didn't realize someone was trying to signal us. After all, a signal is
more
>important than a time out.

a) POSIX does allow timed out thread to consume a signal (cancelled is
not).
b) ETIMEDOUT status just says that: "The time specified by abstime to
pthread_cond_timedwait() has passed."
c) it seem to me that hiding timeouts would violate "The
pthread_cond_timedwait()
function is the same as pthread_cond_wait() except that an error is
returned if
the absolute time specified by abstime passes (that is, system time equals
or
exceeds abstime) before the condition cond is signaled or broadcasted"
because
the abs. time did really pass before cond was signaled (waiter was
released via semaphore). however, if it really matters, i could imaging
that we
can save an abs. time of signal/broadcast and compare it with timeout after
unblock to find out whether it was a 'real' timeout or not. absent this
check
i do think that hiding timeouts would result in technical violation of
specification.. but i think that this check is not important and we can
simply
trust timeout error code provided by wait since we are not trying to make
'hard' realtime implementation.

>What are IPC semaphores?

<sys/sem.h>
int   semctl(int, int, int, ...);
int   semget(key_t, int, int);
int   semop(int, struct sembuf *, size_t);

they support adjustment of semaphore counter (semvalue)
in one single call - imaging Win32 ReleaseSemaphore( hsem,-N )

>In the line where you state, "else if ( nWaitersBlocked > nWaitersGone ) {
>// HARMLESS RACE CONDITION!" there is no race condition for nWaitersGone
>because nWaitersGone is never modified without holding mtxUnblockLock. You
>are correct that there is a harmless race on nWaitersBlocked, which can
>increase and make the condition become true just after we check it. If
this
>happens, we interpret it as the wait starting after the signal.

well, the reason why i've asked on comp.programming.threads whether this
race
condition is harmless or not is that in order to be harmless it should not
violate the waiters visibility rule (see above). Fortunately, we increment
the counter under protection of external mutex.. so that any (signalling)
thread which will acquire the mutex next, should see the updated counter
(in signal) according to POSIX memory visibility rules and mutexes
(memory barriers). But i am not so sure how it actually works on
Win32/INTEL
which does not explicitly define any memory visibility rules :(

>I like your optimization of this. You could improve Alg. 6 as follows:
>---------- Algorithm 6b ----------
>signal(bAll) {
>  _nSig=0
>  lock counters
>  // this is safe because nWaiting can only be decremented by a thread
that
>  // owns counters and nGone can only be changed by a thread that owns
>counters.
>  if (nWaiting>nGone) {
>    if (0==nSignaled) {
>      sema_wait gate // close gate if not already closed
>    }
>    if (nGone>0) {
>      nWaiting-=nGone
>      nGone=0
>    }
>    _nSig=bAll?nWaiting:1
>    nSignaled+=_nSig
>    nWaiting-=_nSig
>  }
>  unlock counters
>  if (0!=_nSig) {
>    sema_post queue, _nSig
>  }
>}
>---------- ---------- ----------
>I guess this wouldn't apply to Alg 8a because nWaitersGone changes
meanings
>depending upon whether the gate is open or closed.

agree.

>In the loop "while ( nWaitersWasGone-- ) {" you do a sema_wait on
>semBlockLock. Perhaps waiting on semBlockQueue would be a better idea.

you are correct. my mistake.

>What have you gained by making the last thread to be signaled do the waits
>for all the timed out threads, besides added complexity? It took me a long
>time to figure out what your objective was with this, to realize you were
>using nWaitersGone to mean two different things, and to verify that you
>hadn't introduced any bug by doing this. Even now I'm not 100% sure.
>
>What has all this playing about with nWaitersGone really gained us besides
a
>lot of complexity (it is much harder to verify that this solution is
>correct), execution overhead (we now have a lot more if statements to
>evaluate), and space overhead (more space for the extra code, and another
>integer in our data)? We did manage to save a lock/unlock pair in an
>uncommon case (when a time out occurs) at the above mentioned expenses in
>the common cases.

well, please consider the following:

1) with multiple waiters unblocked (but some timed out) the trick with
counter
seem to ensure potentially higher level of concurrency by not delaying
most of unblocked waiters for semaphore cleanup - only the last one
will be delayed but all others would already contend/acquire/release
the external mutex - the critical section protected by mtxUnblockLock is
made smaller (increment + couple of IFs is faster than system/kernel call)
which i think is good in general. however, you are right, this is done
at expense of 'normal' waiters..

2) some semaphore APIs (e.g. POSIX IPC sems) do allow to adjust the
semaphore counter in one call => less system/kernel calls.. imagine:

if ( 1 == nSignalsWasLeft ) {
    if ( 0 != nWaitersWasGone ) {
      ReleaseSemaphore( semBlockQueue,-nWaitersWasGone );  // better now
than spurious later
    }
    sem_post( semBlockLock );              // open the gate
  }

3) even on win32 a single thread doing multiple cleanup calls (to wait)
will probably result in faster execution (because of processor caching)
than multiple threads each doing a single call to wait.

>As for 8b, c, and d, they look ok though I haven't studied them
thoroughly.
>What would you use them for?

8b) for semaphores which do not allow to unblock multiple waiters
in a single call to post/release (e.g. POSIX realtime semaphores -
<semaphore.h>)

8c/8d) for WinCE prior to 3.0 (WinCE 3.0 does have semaphores)

ok. so, which one is the 'final' algorithm(s) which we should use in
pthreads-win32??

regards,
alexander.

----------------------------------------------------------------------------

Louis Thomas <lthomas@arbitrade.com> on 02/27/2001 05:20:12 AM

Please respond to Louis Thomas <lthomas@arbitrade.com>

To:   Alexander Terekhov/Germany/IBM@IBMDE
cc:   rpj@ise.canberra.edu.au, Thomas Pfaff <tpfaff@gmx.net>, Nanbor Wang
      <nanbor@cs.wustl.edu>
Subject:  RE: FYI/comp.programming.threads/Re: pthread_cond_* implementatio
      n questions

Sorry all. Busy week.

> this insures the fairness
> which POSIX does not (e.g. two subsequent broadcasts - the gate does
insure
> that first wave waiters will start the race for the mutex before waiters
> from the second wave - Linux pthreads process/unblock both waves
> concurrently...)

I'm not sure how we are any more fair about this than Linux. We certainly
don't guarantee that the threads released by the first broadcast will get
the external mutex before the threads of the second wave. In fact, it is
possible that those threads will never get the external mutex if there is
enough contention for it.

> e.g. i was thinking about implementation with a pool of
> N semaphores/counters [...]

I considered that too. The problem is as you mentioned in a). You really
need to assign threads to semaphores once you know how you want to wake
them
up, not when they first begin waiting which is the only time you can assign
them.

> well, i am not quite sure that i've fully understood your scenario,

Hmm. Well, it think it's an important example, so I'll try again. First, we
have thread A which we KNOW is waiting on a condition. As soon as it
becomes
unblocked for any reason, we will know because it will set a flag. Since
the
flag is not set, we are 100% confident that thread A is waiting on the
condition. We have another thread, thread B, which has acquired the mutex
and is about to wait on the condition. Thus it is pretty clear that at any
point, either just A is waiting, or A and B are waiting. Now thread C comes
along. C is about to do a broadcast on the condition. A broadcast is
guaranteed to unblock all threads currently waiting on a condition, right?
Again, we said that either just A is waiting, or A and B are both waiting.
So, when C does its broadcast, depending upon whether B has started waiting
or not, thread C will unblock A or unblock A and B. Either way, C must
unblock A, right?

Now, you said anything that happens is correct so long as a) "a signal is
not lost between unlocking the mutex and waiting on the condition" and b)
"a
thread must not steal a signal it sent", correct? Requirement b) is easy to
satisfy: in this scenario, thread C will never wait on the condition, so it
won't steal any signals.  Requirement a) is not hard either. The only way
we
could fail to meet requirement a) in this scenario is if thread B was
started waiting but didn't wake up because a signal was lost. This will not
happen.

Now, here is what happens. Assume thread C beats thread B. Thread C looks
to
see how many threads are waiting on the condition. Thread C sees just one
thread, thread A, waiting. It does a broadcast waking up just one thread
because just one thread is waiting. Next, before A can become unblocked,
thread B begins waiting. Now there are two threads waiting, but only one
will be unblocked. Suppose B wins. B will become unblocked. A will not
become unblocked, because C only unblocked one thread (sema_post cond, 1).
So at the end, B finishes and A remains blocked.

We have met both of your requirements, so by your rules, this is an
acceptable outcome. However, I think that the spec says this is an
unacceptable outcome! We know for certain that A was waiting and that C did
a broadcast, but A did not become unblocked! Yet, the spec says that a
broadcast wakes up all waiting threads. This did not happen. Do you agree
that this shows your rules are not strict enough?

> and what about N2? :) this one does allow almost everything.

Don't get me started about rule #2. I'll NEVER advocate an algorithm that
uses rule 2 as an excuse to suck!

> but it is done (decrement)under mutex protection - this is not a subject
> of a race condition.

You are correct. My mistake.

> i would remove "_bTimedOut=false".. after all, it was a real timeout..

I disagree. A thread that can't successfully retract its waiter status
can't
really have timed out. If a thread can't return without executing extra
code
to deal with the fact that someone tried to unblock it, I think it is a
poor
idea to pretend we
didn't realize someone was trying to signal us. After all, a signal is more
important than a time out.

> when nSignaled != 0, it is possible to update nWaiters (--) and do not
> touch nGone

I realize this, but I was thinking that writing it the other ways saves
another if statement.

> adjust only if nGone != 0 and save one cache memory write - probably much
slower than 'if'

Hmm. You are probably right.

> well, in a strange (e.g. timeout test) program you may (theoretically)
> have an overflow of nWaiters/nGone counters (with waiters repeatedly
timing
> out and no signals at all).

Also true. Not only that, but you also have the possibility that one could
overflow the number of waiters as well! However, considering the limit you
have chosen for nWaitersGone, I suppose it is unlikely that anyone would be
able to get INT_MAX/2 threads waiting on a single condition. :)

Analysis of 8a:

It looks correct to me.

What are IPC semaphores?

In the line where you state, "else if ( nWaitersBlocked > nWaitersGone ) {
// HARMLESS RACE CONDITION!" there is no race condition for nWaitersGone
because nWaitersGone is never modified without holding mtxUnblockLock. You
are correct that there is a harmless race on nWaitersBlocked, which can
increase and make the condition become true just after we check it. If this
happens, we interpret it as the wait starting after the signal.

I like your optimization of this. You could improve Alg. 6 as follows:
---------- Algorithm 6b ----------
signal(bAll) {
  _nSig=0
  lock counters
  // this is safe because nWaiting can only be decremented by a thread that
  // owns counters and nGone can only be changed by a thread that owns
counters.
  if (nWaiting>nGone) {
    if (0==nSignaled) {
      sema_wait gate // close gate if not already closed
    }
    if (nGone>0) {
      nWaiting-=nGone
      nGone=0
    }
    _nSig=bAll?nWaiting:1
    nSignaled+=_nSig
    nWaiting-=_nSig
  }
  unlock counters
  if (0!=_nSig) {
    sema_post queue, _nSig
  }
}
---------- ---------- ----------
I guess this wouldn't apply to Alg 8a because nWaitersGone changes meanings
depending upon whether the gate is open or closed.

In the loop "while ( nWaitersWasGone-- ) {" you do a sema_wait on
semBlockLock. Perhaps waiting on semBlockQueue would be a better idea.

What have you gained by making the last thread to be signaled do the waits
for all the timed out threads, besides added complexity? It took me a long
time to figure out what your objective was with this, to realize you were
using nWaitersGone to mean two different things, and to verify that you
hadn't introduced any bug by doing this. Even now I'm not 100% sure.

What has all this playing about with nWaitersGone really gained us besides
a
lot of complexity (it is much harder to verify that this solution is
correct), execution overhead (we now have a lot more if statements to
evaluate), and space overhead (more space for the extra code, and another
integer in our data)? We did manage to save a lock/unlock pair in an
uncommon case (when a time out occurs) at the above mentioned expenses in
the common cases.

As for 8b, c, and d, they look ok though I haven't studied them thoroughly.
What would you use them for?

    Later,
        -Louis! :)

