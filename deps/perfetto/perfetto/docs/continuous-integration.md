# Continuous Integration

This CI is used on-top (not in replacement of) AOSP's TreeHugger.
It gives early testing signals and coverage on other OSes and older Android
devices not supported by TreeHugger.

See the [Testing](testing.md) page for more details about the project testing
strategy.

## Architecture diagram

![Architecture diagram](https://storage.googleapis.com/perfetto/markdown_img/continuous-integration.png)

There are four major components:

1. Frontend: AppEngine.
2. Controller: AppEngine BG service.
3. Workers: Compute Engine + Docker.
4. Database: Firebase realtime database.

They are coupled via the Firebase DB. The DB is the source of truth for the
whole CI.

## Controller

The Controller orchestrates the CI. It's the most trusted piece of the system.

It is based on a background AppEngine service. Such service is only
triggered by deferred tasks and periodic Cron jobs.

The Controller is the only entity which does authenticated access to Gerrit.
It uses a non-privileged gmail account and has no meaningful voting power.

The controller loop does mainly the following:

- It periodically (every 5s) polls Gerrit for CLs updated in the last 24h.
- It checks the list of CLs against the list of already known CLs in the DB.
- For each new CL it enqueues `N` new jobs in the database, one for each
  configuration defined in [config.py](/infra/ci/config.py) (e.g. `linux-debug`,
  `android-release`, ...).
- It monitors the state of jobs. When all jobs for a CL have been completed,
  it posts a comment and adds the vote if the CL is marked as `Presubmit-Ready`.
- It does some other less-relevant bookkeeping.
- AppEngine is highly reliable and self-healing. If a task fails (e.g. because
  of a Gerrit 500) it will be automatically re-tried with exponential backoff.

## Frontend

The frontend is an AppEngine service that hosts the CI website @
[ci.perfetto.dev](https://ci.perfetto.dev).
Conversely to the Controller, it is exposed to the public via HTTP.

- It's an almost fully static website based on HTML and Javascript.
- The only backend-side code ([frontend.py](/infra/ci/frontend/frontend.py))
  is used to proxy XHR GET requests to Gerrit, due to the lack of Gerrit
  CORS headers.
- Such XHR requests are GET-only and anonymous.
- The frontend python code also serves as a memcache layer for Gerrit requests
  that return immutable data (e.g. revision logs) to reduce the likeliness of
  hitting Gerrit errors / timeouts.

## Worker GCE VM

The actual testing job happens inside these Google Compute Engine VMs.
The GCE instance is running a CrOS-based
[Container-Optimized](https://cloud.google.com/container-optimized-os/docs/) OS.

The whole system image is read-only. The VM itself is stateless. No state is
persisted outside of the DB and Google Cloud Storage (only for UI artifacts).
The SSD is used only as a scratch disk and is cleared on each reboot.

VMs are dynamically spawned using the Google Cloud Autoscaler and use a
Stackdriver Custom Metric pushed by the Controller as cost function.
Such metric is the number of queued + running jobs.

Each VM runs two types of Docker containers: _worker_ and the _sandbox_.
They are in a 1:1 relationship, each worker controls at most one sandbox
associated. Workers are always alive (they work in polling-mode), while
sandboxes are started and stopped by the worker on-demand.

On each GCE instance there are M (currently 10) worker containers running and
hence up to M sandboxes.

### Worker containers

Worker containers are trusted entities. They can impersonate the GCE service
account and have R/W access to the DB. They can also spawn sandbox containers.

Their behavior depends only on code that is manually deployed and doesn't depend
on the checkout under test. The reason why workers are Docker containers is NOT
security but only reproducibility and maintenance.

Each worker does the following:

- Poll for an available job from the `/jobs_queued` sub-tree of the DB.
- Move such job into `/jobs_running`.
- Start the sandbox container, passing down the job config and the git revision
  via env vars.
- Stream the sandbox stdout to the `/logs` sub-tree of the DB.
- Terminate the sandbox container prematurely in case of timeouts or job
  cancellations requested by the Controller.
- Upload UI artifacts to GCS.
- Update the DB to reflect completion of jobs, removing the entry from
  `/jobs_running` and updating the `/jobs/$jobId/status` fields.

### Sandbox containers

Sandbox containers are untrusted entities. They can access the internet
(for git pull / install-build-deps) but they cannot impersonate the GCE service
account, cannot write into the DB, cannot write into GCS buckets.
Docker here is used both as an isolation boundary and for reproducibility /
debugging.

Each sandbox does the following:

- Checkout the code at the revision specified in the job config.
- Run one of the [test/ci/](/test/ci/) scripts which will build and run tests.
- Return either a success (0) or fail (!= 0) exit code.

A sandbox container is almost completely stateless with the only exception of
the semi-ephemeral `/ci/cache` mount-point. This mount-point is tmpfs-based
(hence cleared on reboot) but is shared across all sandboxes. It's used only to
maintain the shared ccache.

# Data model

The whole CI is based on
[Firebase Realtime DB](https://firebase.google.com/docs/database).
It is a high-scale JSON object accessible via a simple REST API.
Clients can GET/PUT/PATCH/DELETE individual sub-nodes without having a local
full-copy of the DB.

```bash
/ci
    # For post-submit jobs.
    /branches
        /master-20190626000853
        # ┃     ┗━ Committer-date of the HEAD of the branch.
        # ┗━ Branch name
        {
            author: "primiano@google.com"
            rev: "0552edf491886d2bb6265326a28fef0f73025b6b"
            subject: "Cloud-based CI"
            time_committed: "2019-07-06T02:35:14Z"
            jobs:
            {
                20190708153242--branches-master-20190626000853--android-...: 0
                20190708153242--branches-master-20190626000853--linux-...:  0
                ...
            }
        }
        /master-20190701235742 {...}

    # For pre-submit jobs.
    /cls
        /1000515-65
        {
            change_id:    "platform%2F...~I575be190"
            time_queued:  "2019-07-08T15:32:42Z"
            time_ended:   "2019-07-08T15:33:25Z"
            revision_id:  "18c2e4d0a96..."
            wants_vote:   true
            voted:        true
            jobs: {
                20190708153242--cls-1000515-65--android-clang:  0
                ...
                20190708153242--cls-1000515-65--ui-clang:       0
            }
        }
        /1000515-66 {...}
        ...
        /1011130-3 {...}

    /cls_pending
       # Effectively this is an array of pending CLs that we might need to
       # vote on at the end. Only the keys matter, the values have no
       # semantic and are always 0.
       /1000515-65: 0

    /jobs
        /20190708153242--cls-1000515-65--android-clang-arm-debug:
        #  ┃               ┃             ┗━ Job type.
        #  ┃               ┗━ Path of the CL or branch object.
        #  ┗━ Datetime when the job was created.
        {
            src:          "cls/1000515-66"
            status:       "QUEUED"
                          "STARTED"
                          "COMPLETED"
                          "FAILED"
                          "TIMED_OUT"
                          "CANCELLED"
                          "INTERRUPTED"
            time_ended:   "2019-07-07T12:47:22Z"
            time_queued:  "2019-07-07T12:34:22Z"
            time_started: "2019-07-07T12:34:25Z"
            type:         "android-clang-arm-debug"
            worker:       "zqz2-worker-2"
        }
        /20190707123422--cls-1000515-66--android-clang-arm-rel {..}

    /jobs_queued
        # Effectively this is an array. Only the keys matter, the values
        # have no semantic and are always 0.
        /20190708153242--cls-1000515-65--android-clang-arm-debug: 0

    /jobs_running
        # Effectively this is an array. Only the keys matter, the values
        # have no semantic and are always 0.
        /20190707123422--cls-1000515-66--android-clang-arm-rel

    /logs
        /20190707123422--cls-1000515-66--android-clang-arm-rel
            /00a053-0000: "+ chmod 777 /ci/cache /ci/artifacts"
            # ┃      ┗━ Monotonic counter to establish total order on log lines
            # ┃         retrieved within the same read() batch.
            # ┃
            # ┗━ Hex-encoded timestamp, relative since start of test.
            /00a053-0001: "+ chown perfetto.perfetto /ci/ramdisk"
            ...

```

# Sequence Diagram

This is what happens, in order, on a worker instance from boot to the test run.

```bash
make -C /infra/ci worker-start
┗━ gcloud start ...

[GCE] # From /infra/ci/worker/gce-startup-script.sh
docker run worker-1 ...
...
docker run worker-N ...

[worker-X] # From /infra/ci/worker/Dockerfile
┗━ /infra/ci/worker/worker.py
  ┗━ docker run sandbox-X ...

[sandbox-X] # From /infra/ci/sandbox/Dockerfile
┗━ /infra/ci/sandbox/init.sh
  ┗━ /infra/ci/sandbox/testrunner.sh
    ┣━ git fetch refs/changes/...
    ┇  ...
    ┇  # This env var is passed by the test definition
    ┇  # specified in /infra/ci/config.py .
    ┗━ $PERFETTO_TEST_SCRIPT
       ┣━ # Which is one of these:
       ┣━ /test/ci/android_tests.sh
       ┣━ /test/ci/fuzzer_tests.sh
       ┣━ /test/ci/linux_tests.sh
       ┗━ /test/ci/ui_tests.sh
          ┣━ ninja ...
          ┗━ out/dist/{unit,integration,...}test
```

### [gce-startup-script.sh](/infra/ci/worker/gce-startup-script.sh)

- Is ran once per GVE vm, at (re)boot.
- It prepares the tmpfs mountpoint for the shared ccache.
- It wipes the SSD scratch disk for the build artifacts
- It pulls the latest {worker, sandbox} container images from
  the Google Cloud Container registry.
- Sets up Docker and `iptables` (for the sandboxed network).
- Starts `N` worker containers in Docker.

### [worker.py](/infra/ci/worker/worker.py)

- It polls the DB to retrieve a job.
- When a job is retrieved starts a sandbox container.
- It streams the container stdout/stderr to the DB.
- It upload the build artifacts to GCS.

### [testrunner.sh](/infra/ci/sandbox/testrunner.sh)

- It is pinned in the container image. Does NOT depend on the particular
  revision being tested.
- Checks out the repo at the revision specified (by the Controller) in the
  job config pulled from the DB.
- Sets up ccache
- Deals with caching of buildtools/.
- Runs the test script specified in the job config from the checkout.

### [{android,fuzzer,linux,ui}_tests.sh](/test/ci/linux_tests.sh)

- Are NOT pinned in the container and are ran from the checked out revision.
- Finally build and run the test.

## Playbook

### Frontend (JS/HTML/CSS) changes

Test-locally: `make -C infra/ci/frontend test`

Deploy with `make -C infra/ci/frontend deploy`

### Controller changes

Deploy with `make -C infra/ci/controller deploy`

It is possible to try locally via the `make -C infra/ci/controller test`
but this involves:

- Manually stopping the production AppEngine instance via the Cloud Console
  (stopping via the `gcloud` cli doesn't seem to work, b/136828660)
- Downloading the testing service credentials `test-credentials.json`
  (they are in the internal Team drive).

### Worker/Sandbox changes

1. Build and push the new docker containers with:

   `make -C infra/ci build push`

2. Restart the GCE instances, either manually or via

   `make -C infra/ci restart-workers`


## Security considerations

- Both the Firebase DB and the gs://perfetto-artifacts GCS bucket are
  world-readable and writable by the GAE and GCE service accounts.

- The GAE service account also has the ability to log into Gerrit using a
  dedicated gmail.com account. The GCE service account doesn't.

- Overall, no account in this project has any interesting privilege:
  - The Gerrit account used for commenting on CLs is just a random gmail account
    and has no special voting power.
  - The service accounts of GAE and GCE don't have any special capabilities
    outside of the CI project itself.

- This CI deals only with functional and performance testing and doesn't deal
  with any sort of continuous deployment.

- Presubmit jobs are only triggered if at least one of the following is true:
  - The owner of the CL is a @google.com account.
  - The user that applied the Presubmit-Ready label is a @google.com account.

- Sandboxes are not too hard to escape (Docker is the only boundary) and can
  pollute each other via the shared ccache.

- As such neither pre-submit nor post-submit build artifacts are considered
  trusted. They are only used for establishing functional correctness and
  performance regression testing.

- Binaries built by the CI are not ran on any other machines outside of the
  CI project. They are deliberately not downloadable.

- The only build artifacts that are retained (for up to 30 days) and uploaded to
  the GCS bucket are the UI artifacts. This is for the only sake of getting
  visual previews of the HTML changes.

- UI artifacts are served from a different origin (the GCS per-bucket API) than
  the production UI.
