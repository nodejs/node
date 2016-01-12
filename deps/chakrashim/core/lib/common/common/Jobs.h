//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

// Undefine name #define in OS headers
#undef AddJob
#undef GetJob

class Parser;
class CompileScriptException;

namespace JsUtil
{
    class JobManager;
    class WaitableJobManager;
    class SingleJobManager;
    class WaitableSingleJobManager;
    class JobProcessor;
    class ForegroundJobProcessor;
    class BackgroundJobProcessor;
    struct ParallelThreadData;

    // -------------------------------------------------------------------------------------------------------------------------
    // Job
    //
    // Base class for jobs that can be sent to a job processor for processing
    // -------------------------------------------------------------------------------------------------------------------------

    class Job : public DoublyLinkedListElement<Job>
    {
        friend SingleJobManager;
        friend WaitableSingleJobManager;

    private:
        JobManager *manager;

        // Jobs may be aborted if the job processor is closed while there are still queued jobs, or if a job manager is removed
        // while it still has jobs queued to the job processor. Critical jobs are not aborted and rather processed during the
        // JobProcessor::Close call. Aborted jobs are not processed and instead the job manager is notified with
        // JobManager::JobProcessed(succeeded = false).
        const bool isCritical;

    private:
        Job(const bool isCritical = false);
    public:
        Job(JobManager *const manager, const bool isCritical = false);

#if ENABLE_DEBUG_CONFIG_OPTIONS
        enum class FailureReason
        {
            NotFailed,
            OOM,
            StackOverflow,
            Aborted
        };
        FailureReason failureReason;
#endif

    public:
        JobManager *Manager() const;
        bool IsCritical() const;
    };

    // -------------------------------------------------------------------------------------------------------------------------
    // JobManager
    //
    // Base class for job managers that provide and know how to process jobs. Implementers who wish to be able to wait for a job
    // or job manager should derive from WaitableJobManager.
    // -------------------------------------------------------------------------------------------------------------------------

    class JobManager : public DoublyLinkedListElement<JobManager>
    {
        friend WaitableJobManager;
        friend JobProcessor;
        friend ForegroundJobProcessor;
        friend BackgroundJobProcessor;

    private:
        JobProcessor *const processor;
        unsigned int numJobsAddedToProcessor;

        // Only job managers derived from WaitableJobManager support waiting for a job or the job manager's queued jobs
        const bool isWaitable;

    protected:
        JobManager(JobProcessor *const processor);
    private:
        JobManager(JobProcessor *const processor, const bool isWaitable); // only for use by WaitableJobManager

    public:
        JobProcessor *Processor() const;

    protected:
        // Called by the job processor (outside the lock) to process a job. A job manager may choose to return false to indicate
        // a failure. Throwing OutOfMemoryException or OperationAbortedException also indicate a processing failure.
        // 'pageAllocator' will be null if the job is being processed in the foreground.
        virtual bool Process(Job *const job, ParallelThreadData *threadData) = 0;

        // Called soon after Process by the job processor (inside the lock) to indicate that a job was processed. The
        // 'succeeded' parameter will be false if Process returned false, or threw one of the exceptions that trigger a failure
        // (see comments for Process). A job manager may choose to delete the job during this call.
        virtual void JobProcessed(Job *const job, const bool succeeded) = 0;

        // Called by the job processor (inside the lock) after the last job queued by the job manager to the job processor, is
        // processed. A job manager may choose to remove itself from the job processor and optionally delete itself during this
        // call, provided that it doesn't happen during a call that passes in the job manager as a parameter. For instance, a
        // job manager must not remove or delete itself during any calls to AddManager, RemoveManager, or Prioritize functions.
        virtual void LastJobProcessed();

        // JobProcessor (particularly background jobProcessor) can invoke multiple threads to process the jobs.
        // Manager will get one call back for each spawned thread before the AddManager call completes.
        // This is called outside any lock.
        virtual void ProcessorThreadSpecificCallBack(PageAllocator *pageAllocator);

        // Called when background thread page allocator is decommitted (currently after 1 idle second).
        virtual void OnDecommit(ParallelThreadData *threadData);

        // The following functions are called by the job processor in response to JobProcessor::AddJobAndProcessProactively,
        // PrioritizeManagerAndWait, PrioritizeJob, or PrioritizeJobAndWait. They are not virtual functions because the
        // aforementioned functions are templates that take the actual type of the manager, so derived job managers that wish to
        // use the aforementioned functions should implement these functions (and hence hide the base bare-bones
        // implementations).
        //
        // Job *GetJobToProcessProactively();
        //     Called by the job processor as part of PrioritizeManagerAndWait (outside the lock) when its job queue depletes,
        //     to obtain a job to process proactively. A job manager that does not have a job to provide should return 0. Jobs
        //     provided by this function will be processed in the calling thread, so the job manager must support this.
        //
        // Job *GetJob(const <job holder type> jobHolder) const;
        //     Called in response to PrioritizeJob or PrioritizeJobAndWait (inside the lock), to get the job from the job
        //     holder. The purpose of this is that the job processor does not know the lifetime of a job. When a job manager
        //     asks the job processor to prioritize a job and wait for it, by passing in a job directly, that contract doesn't
        //     make sense because the job manager must ensure that the job object is not deleted during the call. If the job
        //     manager wants to delete jobs after they're processed, it cannot guarantee that. So, the job manager provides a
        //     job holder, and GetJob would then return the associated Job object if it's not yet processed, or 0 if it was
        //     already processed. This function and JobProcessed are both called from inside the lock to enable deterministic
        //     determination of whether a job is already processed, and if it hasn't been processed, to ensure that is won't be
        //     processed until it is prioritized and the lock is released.
        //
        // bool WasAddedToJobProcessor(JsUtil::Job *const job) const;
        //     Called in response to PrioritizeJob or PrioritizeJobAndWait (inside the lock), to determine whether the job to
        //     prioritize was actually added to the job processor. Again, the job processor does not know this if the job
        //     manager has other jobs that are not yet queued to the job processor. However, since doing this check and
        //     prioritizing the job must both happen within a lock, a job manager should always call PrioritizeJob or
        //     PrioritizeJobAndWait on the job processor to prioritize jobs that have or have not been added to the job
        //     processor, and then use the return value of this function to indicate to the job processor whether the job was
        //     added to the job processor.
        //
        // bool ShouldProcessInForeground(const bool willWaitForJob, const unsigned int numJobsInQueue) const;
        //     Called by the BackgroundJobProcessor in response to PrioritizeJob or PrioritizeJobAndWait (inside the lock), to
        //     determine whether the job may be processed in the current (foreground) thread synchronously.
        //
        // void Prioritize(JsUtil::Job *const job, const bool forceAddJobToProcessor = false);
        //     Called in response to PrioritizeJob or PrioritizeJobAndWait (inside the lock), if false is returned from
        //     WasAddedToJobProcessor. If the initial call was PrioritizeJobAndWait, the job processor will pass
        //     forceAddToJobProcessor = true since it needs to wait for the job, and the job manager should ensure to add the
        //     job to the job processor during the Prioritize call in that case.
        //
        // void PrioritizedButNotYetProcessed(JsUtil::Job *const job) const;
        //     Called in response to PrioritizeJob (inside the lock), if the job was not yet processed. May be useful for
        //     tracking how often the job manager asks to prioritize jobs and the jobs are not yet processed. Although the
        //     return value of PrioritizeJob also indicates whether the job was already processed, work done in this function
        //     may need to use the job object, in which case it's necessary to ensure that the job won't be deleted during that
        //     time, and hence the existence of this function and why it's called inside the lock.
        //
        // void BeforeWaitForJob(Js::FunctionBody *const functionBody) const;
        // void AfterWaitForJob(Js::FunctionBody *const functionBody) const;
        //     Called in response to PrioritizeJobAndWait (outside the lock), before and after waiting for the prioritized job
        //     to be processed, respectively.
        //
        // The following are bare-bones implementations of some of the above functions and should be hidden if a job manager
        // wishes to provide its own implementation.
        Job *GetJobToProcessProactively();
        bool ShouldProcessInForeground(const bool willWaitForJob, const unsigned int numJobsInQueue) const;
        void Prioritize(JsUtil::Job *const job, const bool forceAddJobToProcessor = false, void* function = nullptr) const;
        void PrioritizedButNotYetProcessed(JsUtil::Job *const job) const;
        void BeforeWaitForJob(bool) const;
        void AfterWaitForJob(bool) const;
    };

    // -------------------------------------------------------------------------------------------------------------------------
    // WaitableJobManager
    //
    // Base class for job managers that provide and know how to process jobs, and wish to be able to wait for a job or job
    // manager.
    // -------------------------------------------------------------------------------------------------------------------------

    class WaitableJobManager : public JobManager
    {
        friend BackgroundJobProcessor;

    private:
        Job *jobBeingWaitedUpon;
        Event jobBeingWaitedUponProcessed;
        bool isWaitingForQueuedJobs;
        Event queuedJobsProcessed;

    protected:
        WaitableJobManager(JobProcessor *const processor);
    };

    // -------------------------------------------------------------------------------------------------------------------------
    // SingleJobManager
    //
    // Base class for a job manager that queues a single job, to quickly create the job processing implementation inline and
    // queue the job. Since this class derives from JobManager, it does not support waiting and hence, it must be heap-allocated
    // and a derived class should override LastJobProcessed to remove itself from the job processor and delete itself. Derived
    // classes need only override Process and LastJobProcessed.
    // -------------------------------------------------------------------------------------------------------------------------

    class SingleJobManager : public JobManager
    {
        friend ForegroundJobProcessor;
        friend BackgroundJobProcessor;

    private:
        JsUtil::Job job;
        bool processed;

    protected:
        SingleJobManager(JobProcessor *const processor, const bool isCritical = false);

    public:
        void AddJobToProcessor(const bool prioritize);

    protected:
        virtual void JobProcessed(JsUtil::Job *const job, const bool succeeded) override;

        JsUtil::Job *GetJob(bool);
        bool WasAddedToJobProcessor(JsUtil::Job *const job) const;
    };

    // -------------------------------------------------------------------------------------------------------------------------
    // WaitableSingleJobManager
    //
    // Base class for a job manager that queues a single job, to quickly create the job processing implementation inline and
    // queue the job. This class supports waiting for a job and hence, does not delete itself automatically. As a result, it's
    // possible to instantiate this class on the stack, and queue the job and wait for it, all inline without any heap
    // allocation. Derived classes need only override the Process function.
    // -------------------------------------------------------------------------------------------------------------------------

    class WaitableSingleJobManager : public WaitableJobManager
    {
        friend ForegroundJobProcessor;
        friend BackgroundJobProcessor;

    private:
        JsUtil::Job job;
        bool processed;

    protected:
        WaitableSingleJobManager(JobProcessor *const processor, const bool isCritical = false);

    public:
        void AddJobToProcessor(const bool prioritize);
        void WaitForJobProcessed();

    protected:
        virtual void JobProcessed(JsUtil::Job *const job, const bool succeeded) override;

        JsUtil::Job *GetJob(bool);
        bool WasAddedToJobProcessor(JsUtil::Job *const job) const;
    };

    // -------------------------------------------------------------------------------------------------------------------------
    // JobProcessor
    //
    // Base class for job processors.
    // -------------------------------------------------------------------------------------------------------------------------

    class JobProcessor
    {
    private:
        bool processesInBackground;
    protected:
        DoublyLinkedList<JobManager> managers;
        DoublyLinkedList<Job> jobs;
    private:
        bool isClosed;

    protected:
        JobProcessor(const bool processesInBackground);

    public:
        // Ideally, a job manager should not need to depend on this, but there may be cases where it's needed (such as if
        // processing jobs needs to support the -profile switch)
        bool ProcessesInBackground() const;

        // Ideally, a job manager should not need to depend on this, but there may be cases where it's needed (such as if the
        // job manager needs to queue a critical cleanup job to the job processor when it's being destructed, it may need to
        // check first if the job processor is already closed, because it's illegal then)
        bool IsClosed() const;

        // A job manager generally should not need to take the lock, as most functions called by the job processor are called
        // from inside the lock as necessary. The main exception is when adding a job to the job processor; a job manager must
        // call JobProcessor::AddJob inside the lock. Use it sparingly.
        CriticalSection *GetCriticalSection();

        // Adds or removes a job manager
        virtual void AddManager(JobManager *const manager);
        virtual void RemoveManager(JobManager *const manager) = 0;
        bool HasManager(JobManager *const manager) const;
        template<class Fn> void ForEachManager(Fn fn);

        // Prioritizes a job manager, and optionally processes its jobs for a certain amount of time. When a job manager is
        // prioritized, its jobs are moved to the front of the queue. If the queue depletes of this job manager's jobs, the job
        // processor will call JobManager::GetJobToProcessProactively to proactively process jobs that have not yet been queued,
        // until the time limit. See comments in JobManager for details on why templates are used.
        void PrioritizeManager(JobManager *const manager);
        template<class TJobManager> void PrioritizeManagerAndWait(
            TJobManager *const manager,
            const unsigned int milliseconds = INFINITE);

        // Add a job to the queue, and optionally put it in front of the queue. Must be called from inside the lock. A job
        // manager should use JobManager::AcquireLock and JobManager::ReleaseLock for this purpose.
        virtual void AddJob(Job *const job, const bool prioritize = false);

        // Must be called from inside the lock
        virtual bool RemoveJob(Job *const job);

        template<class TJobManager, class TJobHolder>
        void AddJobAndProcessProactively(TJobManager *const jobManager, const TJobHolder holder);

        // Prioritizes a job by moving it to the front of the queue. For details on what more happens during this and on why
        // templates are used, see comments in JobManager. PrioritizeJob returns true if the job was already processed.
        template<class TJobManager, class TJobHolder> bool PrioritizeJob(
            TJobManager *const manager,
            const TJobHolder holder,
            void* function = nullptr);
        template<class TJobManager, class TJobHolder> void PrioritizeJobAndWait(
            TJobManager *const manager,
            const TJobHolder holder,
            void* function = nullptr);

        virtual void AssociatePageAllocator(PageAllocator* const pageAllocator) = 0;
        virtual void DissociatePageAllocator(PageAllocator* const pageAllocator) = 0;

    protected:
        void JobProcessed(JobManager *const manager, Job *const job, const bool succeeded);
        void LastJobProcessed(JobManager *const manager);

    public:
        // Closes the job processor and closes the handle of background threads.
        virtual void Close();
    };

    // -------------------------------------------------------------------------------------------------------------------------
    // ForegroundJobProcessor
    // -------------------------------------------------------------------------------------------------------------------------

    class ForegroundJobProcessor sealed : public JobProcessor
    {
        friend BackgroundJobProcessor;

    public:
        ForegroundJobProcessor();

    public:
        virtual void RemoveManager(JobManager *const manager) override;
        template<class TJobManager> void PrioritizeManagerAndWait(
            TJobManager *const manager,
            const unsigned int milliseconds = INFINITE);

        template<class TJobManager, class TJobHolder>
        void AddJobAndProcessProactively(TJobManager *const jobManager, const TJobHolder holder);

        template<class TJobManager, class TJobHolder> bool PrioritizeJob(
            TJobManager *const manager,
            const TJobHolder holder,
            void* function = nullptr);
        template<class TJobManager, class TJobHolder> void PrioritizeJobAndWait(
            TJobManager *const manager,
            const TJobHolder holder,
            void* function = nullptr);

    private:
        // Calls JobManager::Process, handling specific exception types. The return value indicates whether the job succeeded
        // (true) or failed (false).
        static bool Process(Job *const job);

        virtual void Close() override;

        virtual void AssociatePageAllocator(PageAllocator* const pageAllocator) override;
        virtual void DissociatePageAllocator(PageAllocator* const pageAllocator) override;
    };

    // -------------------------------------------------------------------------------------------------------------------------
    // BackgroundJobProcessor
    // -------------------------------------------------------------------------------------------------------------------------

    struct ParallelThreadData
    {
        HANDLE threadHandle;
        bool isWaitingForJobs;
        bool canDecommit;
        Job *currentJob;
        Event threadStartedOrClosing; //This is only used for shutdown scenario to indicate background thread is shutting down or starting
        PageAllocator backgroundPageAllocator;
        ArenaAllocator *threadArena;
        BackgroundJobProcessor *processor;
        Parser *parser;
        CompileScriptException *pse;

        ParallelThreadData(AllocationPolicyManager* policyManager) :
            threadHandle(0),
            isWaitingForJobs(false),
            canDecommit(true),
            currentJob(nullptr),
            threadStartedOrClosing(false),
            backgroundPageAllocator(policyManager, Js::Configuration::Global.flags, PageAllocatorType_BGJIT,
                                    (AutoSystemInfo::Data.IsLowMemoryProcess() ?
                                     PageAllocator::DefaultLowMaxFreePageCount :
                                     PageAllocator::DefaultMaxFreePageCount)),
            threadArena(nullptr),
            processor(nullptr),
            parser(nullptr),
            pse(nullptr)
            {
            }

        PageAllocator* const GetPageAllocator() { return &backgroundPageAllocator; }
        bool CanDecommit() const { return canDecommit; }
    };

    class BackgroundJobProcessor sealed : public JobProcessor
    {
    private:
        CriticalSection criticalSection;
        Event jobReady;                 //This is an auto reset event, only one thread wakes up when the event is signaled.
        Event wakeAllBackgroundThreads; //This is a manual reset event.
        unsigned int numJobs;
        ThreadContextId threadId;
        ThreadService *threadService;

        unsigned int threadCount;
        unsigned int maxThreadCount;
        ParallelThreadData **parallelThreadData;

#if DBG_DUMP
        static  wchar_t const * const  DebugThreadNames[16];
#endif

    public:
        BackgroundJobProcessor(AllocationPolicyManager* policyManager, ThreadService *threadService, bool disableParallelThreads);
        ~BackgroundJobProcessor();


    private:
        bool WaitWithThread(ParallelThreadData *parallelThreadData, const Event &e, const unsigned int milliseconds = INFINITE);

        bool WaitWithThreadForThreadStartedOrClosingEvent(ParallelThreadData *parallelThreadData, const unsigned int milliseconds = INFINITE);
        void WaitWithAllThreadsForThreadStartedOrClosingEvent();
        bool WaitForJobReadyOrShutdown(ParallelThreadData *threadData); //Returns true for JobReady event is signaled.
        void IndicateNewJob();
        bool AreAllThreadsWaitingForJobs();
        uint NumberOfThreadsWaitingForJobs ();
        Job* GetCurrentJobOfManager(JobManager *const manager);
        ParallelThreadData * GetThreadDataFromCurrentJob(Job* job);

        void InitializeThreadCount();
        void InitializeParallelThreadData(AllocationPolicyManager* policyManager, bool disableParallelThreads);
        void InitializeParallelThreadDataForThreadServiceCallBack(AllocationPolicyManager* policyManager);

    public:
        virtual void AddManager(JobManager *const manager) override;
        virtual void RemoveManager(JobManager *const manager) override;
        template<class Fn> void ForEachManager(Fn fn); //This takes lock for criticalSection
        template<class TJobManager> void PrioritizeManagerAndWait(
            TJobManager *const manager,
            const unsigned int milliseconds = INFINITE);

        virtual void AddJob(Job *const job, const bool prioritize = false) override;
        virtual bool RemoveJob(Job *const job) override;

        template<class TJobManager, class TJobHolder>
        void AddJobAndProcessProactively(TJobManager *const jobManager, const TJobHolder holder);

        template<class TJobManager, class TJobHolder> bool PrioritizeJob(
            TJobManager *const manager,
            const TJobHolder holder,
            void* function = nullptr);
        template<class TJobManager, class TJobHolder> void PrioritizeJobAndWait(
            TJobManager *const manager,
            const TJobHolder holder,
            void* function = nullptr);

        // Calls JobManager::Process, handling specific exception types. The return value indicates whether the job succeeded
        // (true) or failed (false).
        bool Process(Job *const job, ParallelThreadData *threadData);
        bool IsBeingProcessed(Job *job);

        CriticalSection * GetCriticalSection() { return &criticalSection; }

        //Iterates each background thread, callback returns true when it needs to terminate the iteration.
        template<class Fn>
        bool IterateBackgroundThreads(Fn callback)
        {
            for (uint i = 0; i < this->threadCount; i++)
            {
                if (callback(this->parallelThreadData[i]))
                {
                    return false;
                }
            }
            return true;
        }

    private:
        void Run(ParallelThreadData *threadData);

    public:
        virtual void Close() override;
        virtual void AssociatePageAllocator(PageAllocator* const pageAllocator) override;
        virtual void DissociatePageAllocator(PageAllocator* const pageAllocator) override;

    private:
        static unsigned int WINAPI StaticThreadProc(void *lpParam);
        static int ExceptFilter(LPEXCEPTION_POINTERS pEP);

        static void CALLBACK ThreadServiceCallback(void * callbackData);
    };
}
