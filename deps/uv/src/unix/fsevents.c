/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "uv.h"
#include "internal.h"

#if TARGET_OS_IPHONE

/* iOS (currently) doesn't provide the FSEvents-API (nor CoreServices) */

int uv__fsevents_init(uv_fs_event_t* handle) {
  return 0;
}


int uv__fsevents_close(uv_fs_event_t* handle) {
  return 0;
}


void uv__fsevents_loop_delete(uv_loop_t* loop) {
}

#else /* TARGET_OS_IPHONE */

#include <assert.h>
#include <stdlib.h>
#include <pthread.h>

#include <CoreFoundation/CFRunLoop.h>
#include <CoreServices/CoreServices.h>

typedef struct uv__fsevents_event_s uv__fsevents_event_t;
typedef struct uv__cf_loop_signal_s uv__cf_loop_signal_t;
typedef struct uv__cf_loop_state_s uv__cf_loop_state_t;

struct uv__cf_loop_state_s {
  CFRunLoopRef loop;
  CFRunLoopSourceRef signal_source;
  volatile int fsevent_need_reschedule;
  FSEventStreamRef fsevent_stream;
  uv_sem_t fsevent_sem;
  uv_mutex_t fsevent_mutex;
  void* fsevent_handles[2];
  int fsevent_handle_count;
};

struct uv__cf_loop_signal_s {
  QUEUE member;
  uv_fs_event_t* handle;
};

struct uv__fsevents_event_s {
  int events;
  void* next;
  char path[1];
};

static const int kFSEventsModified = kFSEventStreamEventFlagItemFinderInfoMod |
                                     kFSEventStreamEventFlagItemModified |
                                     kFSEventStreamEventFlagItemInodeMetaMod |
                                     kFSEventStreamEventFlagItemChangeOwner |
                                     kFSEventStreamEventFlagItemXattrMod;
static const int kFSEventsRenamed = kFSEventStreamEventFlagItemCreated |
                                    kFSEventStreamEventFlagItemRemoved |
                                    kFSEventStreamEventFlagItemRenamed;
static const int kFSEventsSystem = kFSEventStreamEventFlagUserDropped |
                                   kFSEventStreamEventFlagKernelDropped |
                                   kFSEventStreamEventFlagEventIdsWrapped |
                                   kFSEventStreamEventFlagHistoryDone |
                                   kFSEventStreamEventFlagMount |
                                   kFSEventStreamEventFlagUnmount |
                                   kFSEventStreamEventFlagRootChanged;

/* Forward declarations */
static void uv__cf_loop_cb(void* arg);
static void* uv__cf_loop_runner(void* arg);
static int uv__cf_loop_signal(uv_loop_t* loop, uv_fs_event_t* handle);

#define UV__FSEVENTS_PROCESS(handle, block)                                   \
    do {                                                                      \
      uv__fsevents_event_t* event;                                            \
      uv__fsevents_event_t* next;                                             \
      uv_mutex_lock(&(handle)->cf_mutex);                                     \
      event = (handle)->cf_event;                                             \
      (handle)->cf_event = NULL;                                              \
      uv_mutex_unlock(&(handle)->cf_mutex);                                   \
      while (event != NULL) {                                                 \
        /* Invoke callback */                                                 \
        /* Invoke block code, but only if handle wasn't closed */             \
        if (!uv__is_closing((handle)))                                        \
          block                                                               \
        /* Free allocated data */                                             \
        next = event->next;                                                   \
        free(event);                                                          \
        event = next;                                                         \
      }                                                                       \
    } while (0)


/* Runs in UV loop's thread, when there're events to report to handle */
static void uv__fsevents_cb(uv_async_t* cb, int status) {
  uv_fs_event_t* handle;

  handle = cb->data;

  UV__FSEVENTS_PROCESS(handle, {
    if (handle->event_watcher.fd != -1)
      handle->cb(handle, event->path[0] ? event->path : NULL, event->events, 0);
  });

  if (!uv__is_closing(handle) && handle->event_watcher.fd == -1)
    uv__fsevents_close(handle);
}


/* Runs in CF thread, when there're events in FSEventStream */
static void uv__fsevents_event_cb(ConstFSEventStreamRef streamRef,
                                  void* info,
                                  size_t numEvents,
                                  void* eventPaths,
                                  const FSEventStreamEventFlags eventFlags[],
                                  const FSEventStreamEventId eventIds[]) {
  size_t i;
  int len;
  char** paths;
  char* path;
  char* pos;
  uv_fs_event_t* handle;
  QUEUE* q;
  uv_loop_t* loop;
  uv__cf_loop_state_t* state;
  uv__fsevents_event_t* event;
  uv__fsevents_event_t* tail;

  loop = info;
  state = loop->cf_state;
  assert(state != NULL);
  paths = eventPaths;

  /* For each handle */
  QUEUE_FOREACH(q, &state->fsevent_handles) {
    handle = QUEUE_DATA(q, uv_fs_event_t, cf_member);
    tail = NULL;

    /* Process and filter out events */
    for (i = 0; i < numEvents; i++) {
      /* Ignore system events */
      if (eventFlags[i] & kFSEventsSystem)
        continue;

      path = paths[i];
      len = strlen(path);

      /* Filter out paths that are outside handle's request */
      if (strncmp(path, handle->realpath, handle->realpath_len) != 0)
        continue;

      path += handle->realpath_len;
      len -= handle->realpath_len;

      /* Skip back slash */
      if (*path != 0) {
        path++;
        len--;
      }

#ifdef MAC_OS_X_VERSION_10_7
      /* Ignore events with path equal to directory itself */
      if (len == 0)
        continue;
#endif /* MAC_OS_X_VERSION_10_7 */

      /* Do not emit events from subdirectories (without option set) */
      if ((handle->cf_flags & UV_FS_EVENT_RECURSIVE) == 0) {
        pos = strchr(path, '/');
        if (pos != NULL && pos != path + 1)
          continue;
      }

#ifndef MAC_OS_X_VERSION_10_7
      path = "";
      len = 0;
#endif /* MAC_OS_X_VERSION_10_7 */

      event = malloc(sizeof(*event) + len);
      if (event == NULL)
        break;

      memset(event, 0, sizeof(*event));
      memcpy(event->path, path, len + 1);

      if ((eventFlags[i] & kFSEventsModified) != 0 &&
          (eventFlags[i] & kFSEventsRenamed) == 0)
        event->events = UV_CHANGE;
      else
        event->events = UV_RENAME;

      if (tail != NULL)
        tail->next = event;
      tail = event;
    }

    if (tail != NULL) {
      uv_mutex_lock(&handle->cf_mutex);
      tail->next = handle->cf_event;
      handle->cf_event = tail;
      uv_mutex_unlock(&handle->cf_mutex);

      uv_async_send(handle->cf_cb);
    }
  }
}


/* Runs in CF thread */
static void uv__fsevents_create_stream(uv_loop_t* loop, CFArrayRef paths) {
  uv__cf_loop_state_t* state;
  FSEventStreamContext ctx;
  FSEventStreamRef ref;
  CFAbsoluteTime latency;
  FSEventStreamCreateFlags flags;

  /* Initialize context */
  ctx.version = 0;
  ctx.info = loop;
  ctx.retain = NULL;
  ctx.release = NULL;
  ctx.copyDescription = NULL;

  latency = 0.15;

  /* Set appropriate flags */
  flags = kFSEventStreamCreateFlagFileEvents;

  /*
   * NOTE: It might sound like a good idea to remember last seen StreamEventId,
   * but in reality one dir might have last StreamEventId less than, the other,
   * that is being watched now. Which will cause FSEventStream API to report
   * changes to files from the past.
   */
  ref = FSEventStreamCreate(NULL,
                            &uv__fsevents_event_cb,
                            &ctx,
                            paths,
                            kFSEventStreamEventIdSinceNow,
                            latency,
                            flags);
  assert(ref != NULL);

  state = loop->cf_state;
  FSEventStreamScheduleWithRunLoop(ref,
                                   state->loop,
                                   kCFRunLoopDefaultMode);
  if (!FSEventStreamStart(ref))
    abort();

  state->fsevent_stream = ref;
}


/* Runs in CF thread */
static void uv__fsevents_destroy_stream(uv_loop_t* loop) {
  uv__cf_loop_state_t* state;

  state = loop->cf_state;

  if (state->fsevent_stream == NULL)
    return;

  /* Flush all accumulated events */
  FSEventStreamFlushSync(state->fsevent_stream);

  /* Stop emitting events */
  FSEventStreamStop(state->fsevent_stream);

  /* Release stream */
  FSEventStreamInvalidate(state->fsevent_stream);
  FSEventStreamRelease(state->fsevent_stream);
  state->fsevent_stream = NULL;
}


/* Runs in CF thread, when there're new fsevent handles to add to stream */
static void uv__fsevents_reschedule(uv_fs_event_t* handle) {
  uv__cf_loop_state_t* state;
  QUEUE* q;
  uv_fs_event_t* curr;
  CFArrayRef cf_paths;
  CFStringRef* paths;
  int i;
  int path_count;

  state = handle->loop->cf_state;

  /* Optimization to prevent O(n^2) time spent when starting to watch
   * many files simultaneously
   */
  if (!state->fsevent_need_reschedule)
    return;
  state->fsevent_need_reschedule = 0;

  /* Destroy previous FSEventStream */
  uv__fsevents_destroy_stream(handle->loop);

  /* Create list of all watched paths */
  uv_mutex_lock(&state->fsevent_mutex);
  path_count = state->fsevent_handle_count;
  if (path_count != 0) {
    paths = malloc(sizeof(*paths) * path_count);
    if (paths == NULL)
      abort();

    q = &state->fsevent_handles;
    for (i = 0; i < path_count; i++) {
      q = QUEUE_NEXT(q);
      assert(q != &state->fsevent_handles);
      curr = QUEUE_DATA(q, uv_fs_event_t, cf_member);

      assert(curr->realpath != NULL);
      paths[i] = CFStringCreateWithCString(NULL,
                                           curr->realpath,
                                           CFStringGetSystemEncoding());
      if (paths[i] == NULL)
        abort();
    }
  }
  uv_mutex_unlock(&state->fsevent_mutex);

  if (path_count != 0) {
    /* Create new FSEventStream */
    cf_paths = CFArrayCreate(NULL, (const void**) paths, path_count, NULL);
    if (cf_paths == NULL)
      abort();
    uv__fsevents_create_stream(handle->loop, cf_paths);
  }

  /*
   * Main thread will block until the removal of handle from the list,
   * we must tell it when we're ready.
   *
   * NOTE: This is coupled with `uv_sem_wait()` in `uv__fsevents_close`
   */
  if (uv__is_closing(handle))
    uv_sem_post(&state->fsevent_sem);
}


/* Runs in UV loop */
static int uv__fsevents_loop_init(uv_loop_t* loop) {
  CFRunLoopSourceContext ctx;
  uv__cf_loop_state_t* state;
  pthread_attr_t attr_storage;
  pthread_attr_t* attr;
  int err;

  if (loop->cf_state != NULL)
    return 0;

  state = calloc(1, sizeof(*state));
  if (state == NULL)
    return -ENOMEM;

  err = uv_mutex_init(&loop->cf_mutex);
  if (err)
    return err;

  err = uv_sem_init(&loop->cf_sem, 0);
  if (err)
    goto fail_sem_init;

  QUEUE_INIT(&loop->cf_signals);

  err = uv_sem_init(&state->fsevent_sem, 0);
  if (err)
    goto fail_fsevent_sem_init;

  err = uv_mutex_init(&state->fsevent_mutex);
  if (err)
    goto fail_fsevent_mutex_init;

  QUEUE_INIT(&state->fsevent_handles);
  state->fsevent_need_reschedule = 0;
  state->fsevent_handle_count = 0;

  memset(&ctx, 0, sizeof(ctx));
  ctx.info = loop;
  ctx.perform = uv__cf_loop_cb;
  state->signal_source = CFRunLoopSourceCreate(NULL, 0, &ctx);
  if (state->signal_source == NULL) {
    err = -ENOMEM;
    goto fail_signal_source_create;
  }

  /* In the unlikely event that pthread_attr_init() fails, create the thread
   * with the default stack size. We'll use a little more address space but
   * that in itself is not a fatal error.
   */
  attr = &attr_storage;
  if (pthread_attr_init(attr))
    attr = NULL;

  if (attr != NULL)
    if (pthread_attr_setstacksize(attr, 3 * PTHREAD_STACK_MIN))
      abort();

  loop->cf_state = state;

  /* uv_thread_t is an alias for pthread_t. */
  err = -pthread_create(&loop->cf_thread, attr, uv__cf_loop_runner, loop);

  if (attr != NULL)
    pthread_attr_destroy(attr);

  if (err)
    goto fail_thread_create;

  /* Synchronize threads */
  uv_sem_wait(&loop->cf_sem);
  return 0;

fail_thread_create:
  loop->cf_state = NULL;

fail_signal_source_create:
  uv_mutex_destroy(&state->fsevent_mutex);

fail_fsevent_mutex_init:
  uv_sem_destroy(&state->fsevent_sem);

fail_fsevent_sem_init:
  uv_sem_destroy(&loop->cf_sem);

fail_sem_init:
  uv_mutex_destroy(&loop->cf_mutex);
  free(state);
  return err;
}


/* Runs in UV loop */
void uv__fsevents_loop_delete(uv_loop_t* loop) {
  uv__cf_loop_signal_t* s;
  uv__cf_loop_state_t* state;
  QUEUE* q;

  if (loop->cf_state == NULL)
    return;

  if (uv__cf_loop_signal(loop, NULL) != 0)
    abort();

  uv_thread_join(&loop->cf_thread);
  uv_sem_destroy(&loop->cf_sem);
  uv_mutex_destroy(&loop->cf_mutex);

  /* Free any remaining data */
  while (!QUEUE_EMPTY(&loop->cf_signals)) {
    q = QUEUE_HEAD(&loop->cf_signals);
    s = QUEUE_DATA(q, uv__cf_loop_signal_t, member);
    QUEUE_REMOVE(q);
    free(s);
  }

  /* Destroy state */
  state = loop->cf_state;
  uv_sem_destroy(&state->fsevent_sem);
  uv_mutex_destroy(&state->fsevent_mutex);
  CFRelease(state->signal_source);
  free(state);
  loop->cf_state = NULL;
}


/* Runs in CF thread. This is the CF loop's body */
static void* uv__cf_loop_runner(void* arg) {
  uv_loop_t* loop;
  uv__cf_loop_state_t* state;

  loop = arg;
  state = loop->cf_state;
  state->loop = CFRunLoopGetCurrent();

  CFRunLoopAddSource(state->loop,
                     state->signal_source,
                     kCFRunLoopDefaultMode);

  uv_sem_post(&loop->cf_sem);

  CFRunLoopRun();
  CFRunLoopRemoveSource(state->loop,
                        state->signal_source,
                        kCFRunLoopDefaultMode);

  return NULL;
}


/* Runs in CF thread, executed after `uv__cf_loop_signal()` */
static void uv__cf_loop_cb(void* arg) {
  uv_loop_t* loop;
  uv__cf_loop_state_t* state;
  QUEUE* item;
  QUEUE split_head;
  uv__cf_loop_signal_t* s;

  loop = arg;
  state = loop->cf_state;
  QUEUE_INIT(&split_head);

  uv_mutex_lock(&loop->cf_mutex);
  if (!QUEUE_EMPTY(&loop->cf_signals)) {
    QUEUE* split_pos = QUEUE_HEAD(&loop->cf_signals);
    QUEUE_SPLIT(&loop->cf_signals, split_pos, &split_head);
  }
  uv_mutex_unlock(&loop->cf_mutex);

  while (!QUEUE_EMPTY(&split_head)) {
    item = QUEUE_HEAD(&split_head);

    s = QUEUE_DATA(item, uv__cf_loop_signal_t, member);

    /* This was a termination signal */
    if (s->handle == NULL)
      CFRunLoopStop(state->loop);
    else
      uv__fsevents_reschedule(s->handle);

    QUEUE_REMOVE(item);
    free(s);
  }
}


/* Runs in UV loop to notify CF thread */
int uv__cf_loop_signal(uv_loop_t* loop, uv_fs_event_t* handle) {
  uv__cf_loop_signal_t* item;
  uv__cf_loop_state_t* state;

  item = malloc(sizeof(*item));
  if (item == NULL)
    return -ENOMEM;

  item->handle = handle;

  uv_mutex_lock(&loop->cf_mutex);
  QUEUE_INSERT_TAIL(&loop->cf_signals, &item->member);
  uv_mutex_unlock(&loop->cf_mutex);

  state = loop->cf_state;
  assert(state != NULL);
  CFRunLoopSourceSignal(state->signal_source);
  CFRunLoopWakeUp(state->loop);

  return 0;
}


/* Runs in UV loop to initialize handle */
int uv__fsevents_init(uv_fs_event_t* handle) {
  int err;
  uv__cf_loop_state_t* state;

  err = uv__fsevents_loop_init(handle->loop);
  if (err)
    return err;

  /* Get absolute path to file */
  handle->realpath = realpath(handle->filename, NULL);
  if (handle->realpath == NULL)
    return -errno;
  handle->realpath_len = strlen(handle->realpath);

  /* Initialize singly-linked list */
  handle->cf_event = NULL;

  /*
   * Events will occur in other thread.
   * Initialize callback for getting them back into event loop's thread
   */
  handle->cf_cb = malloc(sizeof(*handle->cf_cb));
  if (handle->cf_cb == NULL) {
    err = -ENOMEM;
    goto fail_cf_cb_malloc;
  }

  handle->cf_cb->data = handle;
  uv_async_init(handle->loop, handle->cf_cb, uv__fsevents_cb);
  handle->cf_cb->flags |= UV__HANDLE_INTERNAL;
  uv_unref((uv_handle_t*) handle->cf_cb);

  err = uv_mutex_init(&handle->cf_mutex);
  if (err)
    goto fail_cf_mutex_init;

  /* Insert handle into the list */
  state = handle->loop->cf_state;
  uv_mutex_lock(&state->fsevent_mutex);
  QUEUE_INSERT_TAIL(&state->fsevent_handles, &handle->cf_member);
  state->fsevent_handle_count++;
  state->fsevent_need_reschedule = 1;
  uv_mutex_unlock(&state->fsevent_mutex);

  /* Reschedule FSEventStream */
  assert(handle != NULL);
  err = uv__cf_loop_signal(handle->loop, handle);
  if (err)
    goto fail_loop_signal;

  return 0;

fail_loop_signal:
  uv_mutex_destroy(&handle->cf_mutex);

fail_cf_mutex_init:
  free(handle->cf_cb);
  handle->cf_cb = NULL;

fail_cf_cb_malloc:
  free(handle->realpath);
  handle->realpath = NULL;
  handle->realpath_len = 0;

  return err;
}


/* Runs in UV loop to de-initialize handle */
int uv__fsevents_close(uv_fs_event_t* handle) {
  int err;
  uv__cf_loop_state_t* state;

  if (handle->cf_cb == NULL)
    return -EINVAL;

  /* Remove handle from  the list */
  state = handle->loop->cf_state;
  uv_mutex_lock(&state->fsevent_mutex);
  QUEUE_REMOVE(&handle->cf_member);
  state->fsevent_handle_count--;
  state->fsevent_need_reschedule = 1;
  uv_mutex_unlock(&state->fsevent_mutex);

  /* Reschedule FSEventStream */
  assert(handle != NULL);
  err = uv__cf_loop_signal(handle->loop, handle);
  if (err)
    return -err;

  /* Wait for deinitialization */
  uv_sem_wait(&state->fsevent_sem);

  uv_close((uv_handle_t*) handle->cf_cb, (uv_close_cb) free);
  handle->cf_cb = NULL;

  /* Free data in queue */
  UV__FSEVENTS_PROCESS(handle, {
    /* NOP */
  });

  uv_mutex_destroy(&handle->cf_mutex);
  free(handle->realpath);
  handle->realpath = NULL;
  handle->realpath_len = 0;

  return 0;
}

#endif /* TARGET_OS_IPHONE */
