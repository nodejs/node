"""SCons.Job

This module defines the Serial and Parallel classes that execute tasks to
complete a build. The Jobs class provides a higher level interface to start,
stop, and wait on jobs.

"""

#
# Copyright (c) 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008 The SCons Foundation
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
# KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
# WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
# LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
# OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
# WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#

__revision__ = "src/engine/SCons/Job.py 3842 2008/12/20 22:59:52 scons"

import os
import signal

import SCons.Errors

# The default stack size (in kilobytes) of the threads used to execute
# jobs in parallel.
#
# We use a stack size of 256 kilobytes. The default on some platforms
# is too large and prevents us from creating enough threads to fully
# parallelized the build. For example, the default stack size on linux
# is 8 MBytes.

explicit_stack_size = None
default_stack_size = 256

interrupt_msg = 'Build interrupted.'


class InterruptState:
   def __init__(self):
       self.interrupted = False

   def set(self):
       self.interrupted = True

   def __call__(self):
       return self.interrupted


class Jobs:
    """An instance of this class initializes N jobs, and provides
    methods for starting, stopping, and waiting on all N jobs.
    """

    def __init__(self, num, taskmaster):
        """
        create 'num' jobs using the given taskmaster.

        If 'num' is 1 or less, then a serial job will be used,
        otherwise a parallel job with 'num' worker threads will
        be used.

        The 'num_jobs' attribute will be set to the actual number of jobs
        allocated.  If more than one job is requested but the Parallel
        class can't do it, it gets reset to 1.  Wrapping interfaces that
        care should check the value of 'num_jobs' after initialization.
        """

        self.job = None
        if num > 1:
            stack_size = explicit_stack_size
            if stack_size is None:
                stack_size = default_stack_size
                
            try:
                self.job = Parallel(taskmaster, num, stack_size)
                self.num_jobs = num
            except NameError:
                pass
        if self.job is None:
            self.job = Serial(taskmaster)
            self.num_jobs = 1

    def run(self, postfunc=lambda: None):
        """Run the jobs.

        postfunc() will be invoked after the jobs has run. It will be
        invoked even if the jobs are interrupted by a keyboard
        interrupt (well, in fact by a signal such as either SIGINT,
        SIGTERM or SIGHUP). The execution of postfunc() is protected
        against keyboard interrupts and is guaranteed to run to
        completion."""
        self._setup_sig_handler()
        try:
            self.job.start()
        finally:
            postfunc()
            self._reset_sig_handler()

    def were_interrupted(self):
        """Returns whether the jobs were interrupted by a signal."""
        return self.job.interrupted()

    def _setup_sig_handler(self):
        """Setup an interrupt handler so that SCons can shutdown cleanly in
        various conditions:

          a) SIGINT: Keyboard interrupt
          b) SIGTERM: kill or system shutdown
          c) SIGHUP: Controlling shell exiting

        We handle all of these cases by stopping the taskmaster. It
        turns out that it very difficult to stop the build process
        by throwing asynchronously an exception such as
        KeyboardInterrupt. For example, the python Condition
        variables (threading.Condition) and Queue's do not seem to
        asynchronous-exception-safe. It would require adding a whole
        bunch of try/finally block and except KeyboardInterrupt all
        over the place.

        Note also that we have to be careful to handle the case when
        SCons forks before executing another process. In that case, we
        want the child to exit immediately.
        """
        def handler(signum, stack, self=self, parentpid=os.getpid()):
            if os.getpid() == parentpid:
                self.job.taskmaster.stop()
                self.job.interrupted.set()
            else:
                os._exit(2)

        self.old_sigint  = signal.signal(signal.SIGINT, handler)
        self.old_sigterm = signal.signal(signal.SIGTERM, handler)
        try:
            self.old_sighup = signal.signal(signal.SIGHUP, handler)
        except AttributeError:
            pass

    def _reset_sig_handler(self):
        """Restore the signal handlers to their previous state (before the
         call to _setup_sig_handler()."""

        signal.signal(signal.SIGINT, self.old_sigint)
        signal.signal(signal.SIGTERM, self.old_sigterm)
        try:
            signal.signal(signal.SIGHUP, self.old_sighup)
        except AttributeError:
            pass

class Serial:
    """This class is used to execute tasks in series, and is more efficient
    than Parallel, but is only appropriate for non-parallel builds. Only
    one instance of this class should be in existence at a time.

    This class is not thread safe.
    """

    def __init__(self, taskmaster):
        """Create a new serial job given a taskmaster. 

        The taskmaster's next_task() method should return the next task
        that needs to be executed, or None if there are no more tasks. The
        taskmaster's executed() method will be called for each task when it
        is successfully executed or failed() will be called if it failed to
        execute (e.g. execute() raised an exception)."""
        
        self.taskmaster = taskmaster
        self.interrupted = InterruptState()

    def start(self):
        """Start the job. This will begin pulling tasks from the taskmaster
        and executing them, and return when there are no more tasks. If a task
        fails to execute (i.e. execute() raises an exception), then the job will
        stop."""
        
        while 1:
            task = self.taskmaster.next_task()

            if task is None:
                break

            try:
                task.prepare()
                if task.needs_execute():
                    task.execute()
            except:
                if self.interrupted():
                    try:
                        raise SCons.Errors.BuildError(
                            task.targets[0], errstr=interrupt_msg)
                    except:
                        task.exception_set()
                else:
                    task.exception_set()

                # Let the failed() callback function arrange for the
                # build to stop if that's appropriate.
                task.failed()
            else:
                task.executed()

            task.postprocess()
        self.taskmaster.cleanup()


# Trap import failure so that everything in the Job module but the
# Parallel class (and its dependent classes) will work if the interpreter
# doesn't support threads.
try:
    import Queue
    import threading
except ImportError:
    pass
else:
    class Worker(threading.Thread):
        """A worker thread waits on a task to be posted to its request queue,
        dequeues the task, executes it, and posts a tuple including the task
        and a boolean indicating whether the task executed successfully. """

        def __init__(self, requestQueue, resultsQueue, interrupted):
            threading.Thread.__init__(self)
            self.setDaemon(1)
            self.requestQueue = requestQueue
            self.resultsQueue = resultsQueue
            self.interrupted = interrupted
            self.start()

        def run(self):
            while 1:
                task = self.requestQueue.get()

                if task is None:
                    # The "None" value is used as a sentinel by
                    # ThreadPool.cleanup().  This indicates that there
                    # are no more tasks, so we should quit.
                    break

                try:
                    if self.interrupted():
                        raise SCons.Errors.BuildError(
                            task.targets[0], errstr=interrupt_msg)
                    task.execute()
                except:
                    task.exception_set()
                    ok = False
                else:
                    ok = True

                self.resultsQueue.put((task, ok))

    class ThreadPool:
        """This class is responsible for spawning and managing worker threads."""

        def __init__(self, num, stack_size, interrupted):
            """Create the request and reply queues, and 'num' worker threads.
            
            One must specify the stack size of the worker threads. The
            stack size is specified in kilobytes.
            """
            self.requestQueue = Queue.Queue(0)
            self.resultsQueue = Queue.Queue(0)

            try:
                prev_size = threading.stack_size(stack_size*1024) 
            except AttributeError, e:
                # Only print a warning if the stack size has been
                # explicitly set.
                if not explicit_stack_size is None:
                    msg = "Setting stack size is unsupported by this version of Python:\n    " + \
                        e.args[0]
                    SCons.Warnings.warn(SCons.Warnings.StackSizeWarning, msg)
            except ValueError, e:
                msg = "Setting stack size failed:\n    " + str(e)
                SCons.Warnings.warn(SCons.Warnings.StackSizeWarning, msg)

            # Create worker threads
            self.workers = []
            for _ in range(num):
                worker = Worker(self.requestQueue, self.resultsQueue, interrupted)
                self.workers.append(worker)

            # Once we drop Python 1.5 we can change the following to:
            #if 'prev_size' in locals():
            if 'prev_size' in locals().keys():
                threading.stack_size(prev_size)

        def put(self, task):
            """Put task into request queue."""
            self.requestQueue.put(task)

        def get(self):
            """Remove and return a result tuple from the results queue."""
            return self.resultsQueue.get()

        def preparation_failed(self, task):
            self.resultsQueue.put((task, False))

        def cleanup(self):
            """
            Shuts down the thread pool, giving each worker thread a
            chance to shut down gracefully.
            """
            # For each worker thread, put a sentinel "None" value
            # on the requestQueue (indicating that there's no work
            # to be done) so that each worker thread will get one and
            # terminate gracefully.
            for _ in self.workers:
                self.requestQueue.put(None)

            # Wait for all of the workers to terminate.
            # 
            # If we don't do this, later Python versions (2.4, 2.5) often
            # seem to raise exceptions during shutdown.  This happens
            # in requestQueue.get(), as an assertion failure that
            # requestQueue.not_full is notified while not acquired,
            # seemingly because the main thread has shut down (or is
            # in the process of doing so) while the workers are still
            # trying to pull sentinels off the requestQueue.
            #
            # Normally these terminations should happen fairly quickly,
            # but we'll stick a one-second timeout on here just in case
            # someone gets hung.
            for worker in self.workers:
                worker.join(1.0)
            self.workers = []

    class Parallel:
        """This class is used to execute tasks in parallel, and is somewhat 
        less efficient than Serial, but is appropriate for parallel builds.

        This class is thread safe.
        """

        def __init__(self, taskmaster, num, stack_size):
            """Create a new parallel job given a taskmaster.

            The taskmaster's next_task() method should return the next
            task that needs to be executed, or None if there are no more
            tasks. The taskmaster's executed() method will be called
            for each task when it is successfully executed or failed()
            will be called if the task failed to execute (i.e. execute()
            raised an exception).

            Note: calls to taskmaster are serialized, but calls to
            execute() on distinct tasks are not serialized, because
            that is the whole point of parallel jobs: they can execute
            multiple tasks simultaneously. """

            self.taskmaster = taskmaster
            self.interrupted = InterruptState()
            self.tp = ThreadPool(num, stack_size, self.interrupted)

            self.maxjobs = num

        def start(self):
            """Start the job. This will begin pulling tasks from the
            taskmaster and executing them, and return when there are no
            more tasks. If a task fails to execute (i.e. execute() raises
            an exception), then the job will stop."""

            jobs = 0
            
            while 1:
                # Start up as many available tasks as we're
                # allowed to.
                while jobs < self.maxjobs:
                    task = self.taskmaster.next_task()
                    if task is None:
                        break

                    try:
                        # prepare task for execution
                        task.prepare()
                    except:
                        task.exception_set()
                        task.failed()
                        task.postprocess()
                    else:
                        if task.needs_execute():
                            # dispatch task
                            self.tp.put(task)
                            jobs = jobs + 1
                        else:
                            task.executed()
                            task.postprocess()

                if not task and not jobs: break

                # Let any/all completed tasks finish up before we go
                # back and put the next batch of tasks on the queue.
                while 1:
                    task, ok = self.tp.get()
                    jobs = jobs - 1

                    if ok:
                        task.executed()
                    else:
                        if self.interrupted():
                            try:
                                raise SCons.Errors.BuildError(
                                    task.targets[0], errstr=interrupt_msg)
                            except:
                                task.exception_set()

                        # Let the failed() callback function arrange
                        # for the build to stop if that's appropriate.
                        task.failed()

                    task.postprocess()

                    if self.tp.resultsQueue.empty():
                        break

            self.tp.cleanup()
            self.taskmaster.cleanup()
