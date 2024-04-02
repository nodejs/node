/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 *
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
#include "task.h"

#include <string.h>
#include <fcntl.h>

#if defined(__APPLE__) && !TARGET_OS_IPHONE
# include <AvailabilityMacros.h>
#endif

#ifndef HAVE_KQUEUE
# if defined(__APPLE__) ||                                                    \
     defined(__DragonFly__) ||                                                \
     defined(__FreeBSD__) ||                                                  \
     defined(__FreeBSD_kernel__) ||                                           \
     defined(__OpenBSD__) ||                                                  \
     defined(__NetBSD__)
#  define HAVE_KQUEUE 1
# endif
#endif

#if defined(__arm__)/* Increase the timeout so the test passes on arm CI bots */
# define CREATE_TIMEOUT 100
#else
# define CREATE_TIMEOUT 1
#endif

static uv_fs_event_t fs_event;
static const char file_prefix[] = "fsevent-";
static const int fs_event_file_count = 16;
#if defined(__APPLE__) || defined(_WIN32)
static const char file_prefix_in_subdir[] = "subdir";
static int fs_multievent_cb_called;
#endif
static uv_timer_t timer;
static int timer_cb_called;
static int close_cb_called;
static int fs_event_created;
static int fs_event_removed;
static int fs_event_cb_called;
#if defined(PATH_MAX)
static char fs_event_filename[PATH_MAX];
#else
static char fs_event_filename[1024];
#endif  /* defined(PATH_MAX) */
static int timer_cb_touch_called;
static int timer_cb_exact_called;

static void fs_event_fail(uv_fs_event_t* handle,
                          const char* filename,
                          int events,
                          int status) {
  ASSERT(0 && "should never be called");
}

static void create_dir(const char* name) {
  int r;
  uv_fs_t req;
  r = uv_fs_mkdir(NULL, &req, name, 0755, NULL);
  ASSERT(r == 0 || r == UV_EEXIST);
  uv_fs_req_cleanup(&req);
}

static void create_file(const char* name) {
  int r;
  uv_file file;
  uv_fs_t req;

  r = uv_fs_open(NULL, &req, name, O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  file = r;
  uv_fs_req_cleanup(&req);
  r = uv_fs_close(NULL, &req, file, NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&req);
}

static void touch_file(const char* name) {
  int r;
  uv_file file;
  uv_fs_t req;
  uv_buf_t buf;

  r = uv_fs_open(NULL, &req, name, O_RDWR, 0, NULL);
  ASSERT(r >= 0);
  file = r;
  uv_fs_req_cleanup(&req);

  buf = uv_buf_init("foo", 4);
  r = uv_fs_write(NULL, &req, file, &buf, 1, -1, NULL);
  ASSERT(r >= 0);
  uv_fs_req_cleanup(&req);

  r = uv_fs_close(NULL, &req, file, NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&req);
}

static void close_cb(uv_handle_t* handle) {
  ASSERT_NOT_NULL(handle);
  close_cb_called++;
}

static void fail_cb(uv_fs_event_t* handle,
                    const char* path,
                    int events,
                    int status) {
  ASSERT(0 && "fail_cb called");
}

static void fs_event_cb_dir(uv_fs_event_t* handle, const char* filename,
  int events, int status) {
  ++fs_event_cb_called;
  ASSERT(handle == &fs_event);
  ASSERT(status == 0);
  ASSERT(events == UV_CHANGE);
  #if defined(__APPLE__) || defined(_WIN32) || defined(__linux__)
  ASSERT(strcmp(filename, "file1") == 0);
  #else
  ASSERT(filename == NULL || strcmp(filename, "file1") == 0);
  #endif
  ASSERT(0 == uv_fs_event_stop(handle));
  uv_close((uv_handle_t*)handle, close_cb);
}

static const char* fs_event_get_filename(int i) {
  snprintf(fs_event_filename,
           sizeof(fs_event_filename),
           "watch_dir/%s%d",
           file_prefix,
           i);
  return fs_event_filename;
}

static void fs_event_create_files(uv_timer_t* handle) {
  /* Make sure we're not attempting to create files we do not intend */
  ASSERT(fs_event_created < fs_event_file_count);

  /* Create the file */
  create_file(fs_event_get_filename(fs_event_created));

  if (++fs_event_created < fs_event_file_count) {
    /* Create another file on a different event loop tick.  We do it this way
     * to avoid fs events coalescing into one fs event. */
    ASSERT(0 == uv_timer_start(&timer,
                               fs_event_create_files,
                               CREATE_TIMEOUT,
                               0));
  }
}

static void fs_event_unlink_files(uv_timer_t* handle) {
  int r;
  int i;

  /* NOTE: handle might be NULL if invoked not as timer callback */
  if (handle == NULL) {
    /* Unlink all files */
    for (i = 0; i < 16; i++) {
      r = remove(fs_event_get_filename(i));
      if (handle != NULL)
        ASSERT(r == 0);
    }
  } else {
    /* Make sure we're not attempting to remove files we do not intend */
    ASSERT(fs_event_removed < fs_event_file_count);

    /* Remove the file */
    ASSERT(0 == remove(fs_event_get_filename(fs_event_removed)));

    if (++fs_event_removed < fs_event_file_count) {
      /* Remove another file on a different event loop tick.  We do it this way
       * to avoid fs events coalescing into one fs event. */
      ASSERT(0 == uv_timer_start(&timer, fs_event_unlink_files, 1, 0));
    }
  }
}

static void fs_event_cb_dir_multi_file(uv_fs_event_t* handle,
                                       const char* filename,
                                       int events,
                                       int status) {
  fs_event_cb_called++;
  ASSERT(handle == &fs_event);
  ASSERT(status == 0);
  ASSERT(events == UV_CHANGE || events == UV_RENAME);
  #if defined(__APPLE__) || defined(_WIN32) || defined(__linux__)
  ASSERT(strncmp(filename, file_prefix, sizeof(file_prefix) - 1) == 0);
  #else
  ASSERT(filename == NULL ||
         strncmp(filename, file_prefix, sizeof(file_prefix) - 1) == 0);
  #endif

  if (fs_event_created + fs_event_removed == fs_event_file_count) {
    /* Once we've processed all create events, delete all files */
    ASSERT(0 == uv_timer_start(&timer, fs_event_unlink_files, 1, 0));
  } else if (fs_event_cb_called == 2 * fs_event_file_count) {
    /* Once we've processed all create and delete events, stop watching */
    uv_close((uv_handle_t*) &timer, close_cb);
    uv_close((uv_handle_t*) handle, close_cb);
  }
}

#if defined(__APPLE__) || defined(_WIN32)
static const char* fs_event_get_filename_in_subdir(int i) {
  snprintf(fs_event_filename,
           sizeof(fs_event_filename),
           "watch_dir/subdir/%s%d",
           file_prefix,
           i);
  return fs_event_filename;
}

static void fs_event_create_files_in_subdir(uv_timer_t* handle) {
  /* Make sure we're not attempting to create files we do not intend */
  ASSERT(fs_event_created < fs_event_file_count);

  /* Create the file */
  create_file(fs_event_get_filename_in_subdir(fs_event_created));

  if (++fs_event_created < fs_event_file_count) {
    /* Create another file on a different event loop tick.  We do it this way
     * to avoid fs events coalescing into one fs event. */
    ASSERT(0 == uv_timer_start(&timer, fs_event_create_files_in_subdir, 1, 0));
  }
}

static void fs_event_unlink_files_in_subdir(uv_timer_t* handle) {
  int r;
  int i;

  /* NOTE: handle might be NULL if invoked not as timer callback */
  if (handle == NULL) {
    /* Unlink all files */
    for (i = 0; i < 16; i++) {
      r = remove(fs_event_get_filename_in_subdir(i));
      if (handle != NULL)
        ASSERT(r == 0);
    }
  } else {
    /* Make sure we're not attempting to remove files we do not intend */
    ASSERT(fs_event_removed < fs_event_file_count);

    /* Remove the file */
    ASSERT(0 == remove(fs_event_get_filename_in_subdir(fs_event_removed)));

    if (++fs_event_removed < fs_event_file_count) {
      /* Remove another file on a different event loop tick.  We do it this way
       * to avoid fs events coalescing into one fs event. */
      ASSERT(0 == uv_timer_start(&timer, fs_event_unlink_files_in_subdir, 1, 0));
    }
  }
}

static void fs_event_cb_dir_multi_file_in_subdir(uv_fs_event_t* handle,
                                                 const char* filename,
                                                 int events,
                                                 int status) {
#ifdef _WIN32
  /* Each file created (or deleted) will cause this callback to be called twice
   * under Windows: once with the name of the file, and second time with the
   * name of the directory. We will ignore the callback for the directory
   * itself. */
  if (filename && strcmp(filename, file_prefix_in_subdir) == 0)
    return;
#endif
  /* It may happen that the "subdir" creation event is captured even though
   * we started watching after its actual creation.
   */
  if (strcmp(filename, "subdir") == 0)
    return;

  fs_multievent_cb_called++;
  ASSERT(handle == &fs_event);
  ASSERT(status == 0);
  ASSERT(events == UV_CHANGE || events == UV_RENAME);
  #if defined(__APPLE__) || defined(_WIN32) || defined(__linux__)
  ASSERT(strncmp(filename,
                 file_prefix_in_subdir,
                 sizeof(file_prefix_in_subdir) - 1) == 0);
  #else
  ASSERT(filename == NULL ||
         strncmp(filename,
                 file_prefix_in_subdir,
                 sizeof(file_prefix_in_subdir) - 1) == 0);
  #endif

  if (fs_event_created == fs_event_file_count &&
      fs_multievent_cb_called == fs_event_created) {
    /* Once we've processed all create events, delete all files */
    ASSERT(0 == uv_timer_start(&timer, fs_event_unlink_files_in_subdir, 1, 0));
  } else if (fs_multievent_cb_called == 2 * fs_event_file_count) {
    /* Once we've processed all create and delete events, stop watching */
    ASSERT(fs_event_removed == fs_event_file_count);
    uv_close((uv_handle_t*) &timer, close_cb);
    uv_close((uv_handle_t*) handle, close_cb);
  }
}
#endif

static void fs_event_cb_file(uv_fs_event_t* handle, const char* filename,
  int events, int status) {
  ++fs_event_cb_called;
  ASSERT(handle == &fs_event);
  ASSERT(status == 0);
  ASSERT(events == UV_CHANGE);
  #if defined(__APPLE__) || defined(_WIN32) || defined(__linux__)
  ASSERT(strcmp(filename, "file2") == 0);
  #else
  ASSERT(filename == NULL || strcmp(filename, "file2") == 0);
  #endif
  ASSERT(0 == uv_fs_event_stop(handle));
  uv_close((uv_handle_t*)handle, close_cb);
}

static void fs_event_cb_file_current_dir(uv_fs_event_t* handle,
  const char* filename, int events, int status) {
  ++fs_event_cb_called;

  ASSERT(handle == &fs_event);
  ASSERT(status == 0);
  ASSERT(events == UV_CHANGE);
  #if defined(__APPLE__) || defined(_WIN32) || defined(__linux__)
  ASSERT(strcmp(filename, "watch_file") == 0);
  #else
  ASSERT(filename == NULL || strcmp(filename, "watch_file") == 0);
  #endif

  uv_close((uv_handle_t*)handle, close_cb);
}

static void timer_cb_file(uv_timer_t* handle) {
  ++timer_cb_called;

  if (timer_cb_called == 1) {
    touch_file("watch_dir/file1");
  } else {
    touch_file("watch_dir/file2");
    uv_close((uv_handle_t*)handle, close_cb);
  }
}

static void timer_cb_touch(uv_timer_t* timer) {
  uv_close((uv_handle_t*)timer, NULL);
  touch_file((char*) timer->data);
  timer_cb_touch_called++;
}

static void timer_cb_exact(uv_timer_t* handle) {
  int r;

  if (timer_cb_exact_called == 0) {
    touch_file("watch_dir/file.js");
  } else {
    uv_close((uv_handle_t*)handle, NULL);
    r = uv_fs_event_stop(&fs_event);
    ASSERT(r == 0);
    uv_close((uv_handle_t*) &fs_event, NULL);
  }

  ++timer_cb_exact_called;
}

static void timer_cb_watch_twice(uv_timer_t* handle) {
  uv_fs_event_t* handles = handle->data;
  uv_close((uv_handle_t*) (handles + 0), NULL);
  uv_close((uv_handle_t*) (handles + 1), NULL);
  uv_close((uv_handle_t*) handle, NULL);
}

static void fs_event_cb_close(uv_fs_event_t* handle,
                              const char* filename,
                              int events,
                              int status) {
  ASSERT(status == 0);

  ASSERT(fs_event_cb_called < 3);
  ++fs_event_cb_called;

  if (fs_event_cb_called == 3) {
    uv_close((uv_handle_t*) handle, close_cb);
  }
}


TEST_IMPL(fs_event_watch_dir) {
#if defined(NO_FS_EVENTS)
  RETURN_SKIP(NO_FS_EVENTS);
#elif defined(__MVS__)
  RETURN_SKIP("Directory watching not supported on this platform.");
#endif

  uv_loop_t* loop = uv_default_loop();
  int r;

  /* Setup */
  fs_event_unlink_files(NULL);
  remove("watch_dir/file2");
  remove("watch_dir/file1");
  remove("watch_dir/");
  create_dir("watch_dir");

  r = uv_fs_event_init(loop, &fs_event);
  ASSERT(r == 0);
  r = uv_fs_event_start(&fs_event, fs_event_cb_dir_multi_file, "watch_dir", 0);
  ASSERT(r == 0);
  r = uv_timer_init(loop, &timer);
  ASSERT(r == 0);
  r = uv_timer_start(&timer, fs_event_create_files, 100, 0);
  ASSERT(r == 0);

  uv_run(loop, UV_RUN_DEFAULT);

  ASSERT(fs_event_cb_called == fs_event_created + fs_event_removed);
  ASSERT(close_cb_called == 2);

  /* Cleanup */
  fs_event_unlink_files(NULL);
  remove("watch_dir/file2");
  remove("watch_dir/file1");
  remove("watch_dir/");

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(fs_event_watch_dir_recursive) {
#if defined(__APPLE__) || defined(_WIN32)
  uv_loop_t* loop;
  int r;
  uv_fs_event_t fs_event_root;

  /* Setup */
  loop = uv_default_loop();
  fs_event_unlink_files(NULL);
  remove("watch_dir/file2");
  remove("watch_dir/file1");
  remove("watch_dir/subdir");
  remove("watch_dir/");
  create_dir("watch_dir");
  create_dir("watch_dir/subdir");

  r = uv_fs_event_init(loop, &fs_event);
  ASSERT(r == 0);
  r = uv_fs_event_start(&fs_event,
                        fs_event_cb_dir_multi_file_in_subdir,
                        "watch_dir",
                        UV_FS_EVENT_RECURSIVE);
  ASSERT(r == 0);
  r = uv_timer_init(loop, &timer);
  ASSERT(r == 0);
  r = uv_timer_start(&timer, fs_event_create_files_in_subdir, 100, 0);
  ASSERT(r == 0);

#ifndef _WIN32
  /* Also try to watch the root directory.
   * This will be noisier, so we're just checking for any couple events to happen. */
  r = uv_fs_event_init(loop, &fs_event_root);
  ASSERT(r == 0);
  r = uv_fs_event_start(&fs_event_root,
                        fs_event_cb_close,
                        "/",
                        UV_FS_EVENT_RECURSIVE);
  ASSERT(r == 0);
#else
  fs_event_cb_called += 3;
  close_cb_called += 1;
  (void)fs_event_root;
#endif

  uv_run(loop, UV_RUN_DEFAULT);

  ASSERT(fs_multievent_cb_called == fs_event_created + fs_event_removed);
  ASSERT(fs_event_cb_called == 3);
  ASSERT(close_cb_called == 3);

  /* Cleanup */
  fs_event_unlink_files_in_subdir(NULL);
  remove("watch_dir/file2");
  remove("watch_dir/file1");
  remove("watch_dir/subdir");
  remove("watch_dir/");

  MAKE_VALGRIND_HAPPY();
  return 0;
#else
  RETURN_SKIP("Recursive directory watching not supported on this platform.");
#endif
}

#ifdef _WIN32
TEST_IMPL(fs_event_watch_dir_short_path) {
  uv_loop_t* loop;
  uv_fs_t req;
  int has_shortnames;
  int r;

  /* Setup */
  loop = uv_default_loop();
  remove("watch_dir/file1");
  remove("watch_dir/");
  create_dir("watch_dir");
  create_file("watch_dir/file1");

  /* Newer version of Windows ship with
     HKLM\SYSTEM\CurrentControlSet\Control\FileSystem\NtfsDisable8dot3NameCreation
     not equal to 0. So we verify the files we created are addressable by a 8.3
     short name */
  has_shortnames = uv_fs_stat(NULL, &req, "watch_~1", NULL) != UV_ENOENT;
  if (has_shortnames) {
    r = uv_fs_event_init(loop, &fs_event);
    ASSERT(r == 0);
    r = uv_fs_event_start(&fs_event, fs_event_cb_dir, "watch_~1", 0);
    ASSERT(r == 0);
    r = uv_timer_init(loop, &timer);
    ASSERT(r == 0);
    r = uv_timer_start(&timer, timer_cb_file, 100, 0);
    ASSERT(r == 0);

    uv_run(loop, UV_RUN_DEFAULT);

    ASSERT(fs_event_cb_called == 1);
    ASSERT(timer_cb_called == 1);
    ASSERT(close_cb_called == 1);
  }

  /* Cleanup */
  remove("watch_dir/file1");
  remove("watch_dir/");

  MAKE_VALGRIND_HAPPY();

  if (!has_shortnames)
    RETURN_SKIP("Was not able to address files with 8.3 short name.");

  return 0;
}
#endif


TEST_IMPL(fs_event_watch_file) {
#if defined(NO_FS_EVENTS)
  RETURN_SKIP(NO_FS_EVENTS);
#endif

  uv_loop_t* loop = uv_default_loop();
  int r;

  /* Setup */
  remove("watch_dir/file2");
  remove("watch_dir/file1");
  remove("watch_dir/");
  create_dir("watch_dir");
  create_file("watch_dir/file1");
  create_file("watch_dir/file2");

  r = uv_fs_event_init(loop, &fs_event);
  ASSERT(r == 0);
  r = uv_fs_event_start(&fs_event, fs_event_cb_file, "watch_dir/file2", 0);
  ASSERT(r == 0);
  r = uv_timer_init(loop, &timer);
  ASSERT(r == 0);
  r = uv_timer_start(&timer, timer_cb_file, 100, 100);
  ASSERT(r == 0);

  uv_run(loop, UV_RUN_DEFAULT);

  ASSERT(fs_event_cb_called == 1);
  ASSERT(timer_cb_called == 2);
  ASSERT(close_cb_called == 2);

  /* Cleanup */
  remove("watch_dir/file2");
  remove("watch_dir/file1");
  remove("watch_dir/");

  MAKE_VALGRIND_HAPPY();
  return 0;
}

TEST_IMPL(fs_event_watch_file_exact_path) {
  /*
    This test watches a file named "file.jsx" and modifies a file named
    "file.js". The test verifies that no events occur for file.jsx.
  */

#if defined(NO_FS_EVENTS)
  RETURN_SKIP(NO_FS_EVENTS);
#endif

  uv_loop_t* loop;
  int r;

  loop = uv_default_loop();

  /* Setup */
  remove("watch_dir/file.js");
  remove("watch_dir/file.jsx");
  remove("watch_dir/");
  create_dir("watch_dir");
  create_file("watch_dir/file.js");
  create_file("watch_dir/file.jsx");
#if defined(__APPLE__) && !defined(MAC_OS_X_VERSION_10_12)
  /* Empirically, FSEvents seems to (reliably) report the preceeding
   * create_file events prior to macOS 10.11.6 in the subsequent fs_watch
   * creation, but that behavior hasn't been observed to occur on newer
   * versions. Give a long delay here to let the system settle before running
   * the test. */
  uv_sleep(1100);
  uv_update_time(loop);
#endif

  r = uv_fs_event_init(loop, &fs_event);
  ASSERT(r == 0);
  r = uv_fs_event_start(&fs_event, fs_event_fail, "watch_dir/file.jsx", 0);
  ASSERT(r == 0);
  r = uv_timer_init(loop, &timer);
  ASSERT(r == 0);
  r = uv_timer_start(&timer, timer_cb_exact, 100, 100);
  ASSERT(r == 0);
  r = uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(r == 0);
  ASSERT(timer_cb_exact_called == 2);

  /* Cleanup */
  remove("watch_dir/file.js");
  remove("watch_dir/file.jsx");
  remove("watch_dir/");

  MAKE_VALGRIND_HAPPY();
  return 0;
}

TEST_IMPL(fs_event_watch_file_twice) {
#if defined(NO_FS_EVENTS)
  RETURN_SKIP(NO_FS_EVENTS);
#endif
  const char path[] = "test/fixtures/empty_file";
  uv_fs_event_t watchers[2];
  uv_timer_t timer;
  uv_loop_t* loop;

  loop = uv_default_loop();
  timer.data = watchers;

  ASSERT(0 == uv_fs_event_init(loop, watchers + 0));
  ASSERT(0 == uv_fs_event_start(watchers + 0, fail_cb, path, 0));
  ASSERT(0 == uv_fs_event_init(loop, watchers + 1));
  ASSERT(0 == uv_fs_event_start(watchers + 1, fail_cb, path, 0));
  ASSERT(0 == uv_timer_init(loop, &timer));
  ASSERT(0 == uv_timer_start(&timer, timer_cb_watch_twice, 10, 0));
  ASSERT(0 == uv_run(loop, UV_RUN_DEFAULT));

  MAKE_VALGRIND_HAPPY();
  return 0;
}

TEST_IMPL(fs_event_watch_file_current_dir) {
#if defined(NO_FS_EVENTS)
  RETURN_SKIP(NO_FS_EVENTS);
#endif
  uv_timer_t timer;
  uv_loop_t* loop;
  int r;

  loop = uv_default_loop();

  /* Setup */
  remove("watch_file");
  create_file("watch_file");
#if defined(__APPLE__) && !defined(MAC_OS_X_VERSION_10_12)
  /* Empirically, kevent seems to (sometimes) report the preceeding
   * create_file events prior to macOS 10.11.6 in the subsequent fs_event_start
   * So let the system settle before running the test. */
  uv_sleep(1100);
  uv_update_time(loop);
#endif

  r = uv_fs_event_init(loop, &fs_event);
  ASSERT(r == 0);
  r = uv_fs_event_start(&fs_event,
                        fs_event_cb_file_current_dir,
                        "watch_file",
                        0);
  ASSERT(r == 0);


  r = uv_timer_init(loop, &timer);
  ASSERT(r == 0);

  timer.data = "watch_file";
  r = uv_timer_start(&timer, timer_cb_touch, 1100, 0);
  ASSERT(r == 0);

  ASSERT(timer_cb_touch_called == 0);
  ASSERT(fs_event_cb_called == 0);
  ASSERT(close_cb_called == 0);

  uv_run(loop, UV_RUN_DEFAULT);

  ASSERT(timer_cb_touch_called == 1);
  /* FSEvents on macOS sometimes sends one change event, sometimes two. */
  ASSERT_NE(0, fs_event_cb_called);
  ASSERT(close_cb_called == 1);

  /* Cleanup */
  remove("watch_file");

  MAKE_VALGRIND_HAPPY();
  return 0;
}

#ifdef _WIN32
TEST_IMPL(fs_event_watch_file_root_dir) {
  uv_loop_t* loop;
  int r;

  const char* sys_drive = getenv("SystemDrive");
  char path[] = "\\\\?\\X:\\bootsect.bak";

  ASSERT_NOT_NULL(sys_drive);
  strncpy(path + sizeof("\\\\?\\") - 1, sys_drive, 1);

  loop = uv_default_loop();

  r = uv_fs_event_init(loop, &fs_event);
  ASSERT(r == 0);
  r = uv_fs_event_start(&fs_event, fail_cb, path, 0);
  if (r == UV_ENOENT)
    RETURN_SKIP("bootsect.bak doesn't exist in system root.\n");
  ASSERT(r == 0);

  uv_close((uv_handle_t*) &fs_event, NULL);

  MAKE_VALGRIND_HAPPY();
  return 0;
}
#endif

TEST_IMPL(fs_event_no_callback_after_close) {
#if defined(NO_FS_EVENTS)
  RETURN_SKIP(NO_FS_EVENTS);
#endif

  uv_loop_t* loop = uv_default_loop();
  int r;

  /* Setup */
  remove("watch_dir/file1");
  remove("watch_dir/");
  create_dir("watch_dir");
  create_file("watch_dir/file1");

  r = uv_fs_event_init(loop, &fs_event);
  ASSERT(r == 0);
  r = uv_fs_event_start(&fs_event,
                        fs_event_cb_file,
                        "watch_dir/file1",
                        0);
  ASSERT(r == 0);


  uv_close((uv_handle_t*)&fs_event, close_cb);
  touch_file("watch_dir/file1");
  uv_run(loop, UV_RUN_DEFAULT);

  ASSERT(fs_event_cb_called == 0);
  ASSERT(close_cb_called == 1);

  /* Cleanup */
  remove("watch_dir/file1");
  remove("watch_dir/");

  MAKE_VALGRIND_HAPPY();
  return 0;
}

TEST_IMPL(fs_event_no_callback_on_close) {
#if defined(NO_FS_EVENTS)
  RETURN_SKIP(NO_FS_EVENTS);
#endif

  uv_loop_t* loop = uv_default_loop();
  int r;

  /* Setup */
  remove("watch_dir/file1");
  remove("watch_dir/");
  create_dir("watch_dir");
  create_file("watch_dir/file1");

  r = uv_fs_event_init(loop, &fs_event);
  ASSERT(r == 0);
  r = uv_fs_event_start(&fs_event,
                        fs_event_cb_file,
                        "watch_dir/file1",
                        0);
  ASSERT(r == 0);

  uv_close((uv_handle_t*)&fs_event, close_cb);

  uv_run(loop, UV_RUN_DEFAULT);

  ASSERT(fs_event_cb_called == 0);
  ASSERT(close_cb_called == 1);

  /* Cleanup */
  remove("watch_dir/file1");
  remove("watch_dir/");

  MAKE_VALGRIND_HAPPY();
  return 0;
}


static void timer_cb(uv_timer_t* handle) {
  int r;

  r = uv_fs_event_init(handle->loop, &fs_event);
  ASSERT(r == 0);
  r = uv_fs_event_start(&fs_event, fs_event_fail, ".", 0);
  ASSERT(r == 0);

  uv_close((uv_handle_t*)&fs_event, close_cb);
  uv_close((uv_handle_t*)handle, close_cb);
}


TEST_IMPL(fs_event_immediate_close) {
#if defined(NO_FS_EVENTS)
  RETURN_SKIP(NO_FS_EVENTS);
#endif
  uv_timer_t timer;
  uv_loop_t* loop;
  int r;

  loop = uv_default_loop();

  r = uv_timer_init(loop, &timer);
  ASSERT(r == 0);

  r = uv_timer_start(&timer, timer_cb, 1, 0);
  ASSERT(r == 0);

  uv_run(loop, UV_RUN_DEFAULT);

  ASSERT(close_cb_called == 2);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(fs_event_close_with_pending_event) {
#if defined(NO_FS_EVENTS)
  RETURN_SKIP(NO_FS_EVENTS);
#endif
  uv_loop_t* loop;
  int r;

  loop = uv_default_loop();

  create_dir("watch_dir");
  create_file("watch_dir/file");

  r = uv_fs_event_init(loop, &fs_event);
  ASSERT(r == 0);
  r = uv_fs_event_start(&fs_event, fs_event_fail, "watch_dir", 0);
  ASSERT(r == 0);

  /* Generate an fs event. */
  touch_file("watch_dir/file");

  uv_close((uv_handle_t*)&fs_event, close_cb);

  uv_run(loop, UV_RUN_DEFAULT);

  ASSERT(close_cb_called == 1);

  /* Clean up */
  remove("watch_dir/file");
  remove("watch_dir/");

  MAKE_VALGRIND_HAPPY();
  return 0;
}

TEST_IMPL(fs_event_close_with_pending_delete_event) {
#if defined(NO_FS_EVENTS)
  RETURN_SKIP(NO_FS_EVENTS);
#endif
  uv_loop_t* loop;
  int r;

  loop = uv_default_loop();

  create_dir("watch_dir");
  create_file("watch_dir/file");

  r = uv_fs_event_init(loop, &fs_event);
  ASSERT(r == 0);
  r = uv_fs_event_start(&fs_event, fs_event_fail, "watch_dir/file", 0);
  ASSERT(r == 0);

  /* Generate an fs event. */
  remove("watch_dir/file");

  /* Allow time for the remove event to propagate to the pending list. */
  /* XXX - perhaps just for __sun? */
  uv_sleep(1100);
  uv_update_time(loop);

  uv_close((uv_handle_t*)&fs_event, close_cb);

  uv_run(loop, UV_RUN_DEFAULT);

  ASSERT(close_cb_called == 1);

  /* Clean up */
  remove("watch_dir/");

  MAKE_VALGRIND_HAPPY();
  return 0;
}

TEST_IMPL(fs_event_close_in_callback) {
#if defined(NO_FS_EVENTS)
  RETURN_SKIP(NO_FS_EVENTS);
#elif defined(__MVS__)
  RETURN_SKIP("Directory watching not supported on this platform.");
#endif
  uv_loop_t* loop;
  int r;

  loop = uv_default_loop();

  fs_event_unlink_files(NULL);
  create_dir("watch_dir");

  r = uv_fs_event_init(loop, &fs_event);
  ASSERT(r == 0);
  r = uv_fs_event_start(&fs_event, fs_event_cb_close, "watch_dir", 0);
  ASSERT(r == 0);

  r = uv_timer_init(loop, &timer);
  ASSERT(r == 0);
  r = uv_timer_start(&timer, fs_event_create_files, 100, 0);
  ASSERT(r == 0);

  uv_run(loop, UV_RUN_DEFAULT);

  uv_close((uv_handle_t*)&timer, close_cb);

  uv_run(loop, UV_RUN_ONCE);

  ASSERT(close_cb_called == 2);
  ASSERT(fs_event_cb_called == 3);

  /* Clean up */
  fs_event_unlink_files(NULL);
  remove("watch_dir/");

  MAKE_VALGRIND_HAPPY();
  return 0;
}

TEST_IMPL(fs_event_start_and_close) {
#if defined(NO_FS_EVENTS)
  RETURN_SKIP(NO_FS_EVENTS);
#endif
  uv_loop_t* loop;
  uv_fs_event_t fs_event1;
  uv_fs_event_t fs_event2;
  int r;

  loop = uv_default_loop();

  create_dir("watch_dir");

  r = uv_fs_event_init(loop, &fs_event1);
  ASSERT(r == 0);
  r = uv_fs_event_start(&fs_event1, fs_event_cb_dir, "watch_dir", 0);
  ASSERT(r == 0);

  r = uv_fs_event_init(loop, &fs_event2);
  ASSERT(r == 0);
  r = uv_fs_event_start(&fs_event2, fs_event_cb_dir, "watch_dir", 0);
  ASSERT(r == 0);

  uv_close((uv_handle_t*) &fs_event2, close_cb);
  uv_close((uv_handle_t*) &fs_event1, close_cb);

  uv_run(loop, UV_RUN_DEFAULT);

  ASSERT(close_cb_called == 2);

  remove("watch_dir/");
  MAKE_VALGRIND_HAPPY();
  return 0;
}

TEST_IMPL(fs_event_getpath) {
#if defined(NO_FS_EVENTS)
  RETURN_SKIP(NO_FS_EVENTS);
#endif
  uv_loop_t* loop = uv_default_loop();
  unsigned i;
  int r;
  char buf[1024];
  size_t len;
  const char* const watch_dir[] = {
    "watch_dir",
    "watch_dir/",
    "watch_dir///",
    "watch_dir/subfolder/..",
    "watch_dir//subfolder//..//",
  };

  create_dir("watch_dir");
  create_dir("watch_dir/subfolder");


  for (i = 0; i < ARRAY_SIZE(watch_dir); i++) {
    r = uv_fs_event_init(loop, &fs_event);
    ASSERT(r == 0);
    len = sizeof buf;
    r = uv_fs_event_getpath(&fs_event, buf, &len);
    ASSERT(r == UV_EINVAL);
    r = uv_fs_event_start(&fs_event, fail_cb, watch_dir[i], 0);
    ASSERT(r == 0);
    len = 0;
    r = uv_fs_event_getpath(&fs_event, buf, &len);
    ASSERT(r == UV_ENOBUFS);
    ASSERT(len < sizeof buf); /* sanity check */
    ASSERT(len == strlen(watch_dir[i]) + 1);
    r = uv_fs_event_getpath(&fs_event, buf, &len);
    ASSERT(r == 0);
    ASSERT(len == strlen(watch_dir[i]));
    ASSERT(strcmp(buf, watch_dir[i]) == 0);
    r = uv_fs_event_stop(&fs_event);
    ASSERT(r == 0);
    uv_close((uv_handle_t*) &fs_event, close_cb);

    uv_run(loop, UV_RUN_DEFAULT);

    ASSERT(close_cb_called == 1);
    close_cb_called = 0;
  }

  remove("watch_dir/");
  MAKE_VALGRIND_HAPPY();
  return 0;
}

#if defined(__APPLE__)

static int fs_event_error_reported;

static void fs_event_error_report_cb(uv_fs_event_t* handle,
                                     const char* filename,
                                     int events,
                                     int status) {
  if (status != 0)
    fs_event_error_reported = status;
}

static void timer_cb_nop(uv_timer_t* handle) {
  ++timer_cb_called;
  uv_close((uv_handle_t*) handle, close_cb);
}

static void fs_event_error_report_close_cb(uv_handle_t* handle) {
  ASSERT_NOT_NULL(handle);
  close_cb_called++;

  /* handle is allocated on-stack, no need to free it */
}


TEST_IMPL(fs_event_error_reporting) {
  unsigned int i;
  uv_loop_t loops[1024];
  uv_fs_event_t events[ARRAY_SIZE(loops)];
  uv_loop_t* loop;
  uv_fs_event_t* event;

  TEST_FILE_LIMIT(ARRAY_SIZE(loops) * 3);

  remove("watch_dir/");
  create_dir("watch_dir");

  /* Create a lot of loops, and start FSEventStream in each of them.
   * Eventually, this should create enough streams to make FSEventStreamStart()
   * fail.
   */
  for (i = 0; i < ARRAY_SIZE(loops); i++) {
    loop = &loops[i];
    ASSERT(0 == uv_loop_init(loop));
    event = &events[i];

    timer_cb_called = 0;
    close_cb_called = 0;
    ASSERT(0 == uv_fs_event_init(loop, event));
    ASSERT(0 == uv_fs_event_start(event,
                                  fs_event_error_report_cb,
                                  "watch_dir",
                                  0));
    uv_unref((uv_handle_t*) event);

    /* Let loop run for some time */
    ASSERT(0 == uv_timer_init(loop, &timer));
    ASSERT(0 == uv_timer_start(&timer, timer_cb_nop, 2, 0));
    uv_run(loop, UV_RUN_DEFAULT);
    ASSERT(1 == timer_cb_called);
    ASSERT(1 == close_cb_called);
    if (fs_event_error_reported != 0)
      break;
  }

  /* At least one loop should fail */
  ASSERT(fs_event_error_reported == UV_EMFILE);

  /* Stop and close all events, and destroy loops */
  do {
    loop = &loops[i];
    event = &events[i];

    ASSERT(0 == uv_fs_event_stop(event));
    uv_ref((uv_handle_t*) event);
    uv_close((uv_handle_t*) event, fs_event_error_report_close_cb);

    close_cb_called = 0;
    uv_run(loop, UV_RUN_DEFAULT);
    ASSERT(close_cb_called == 1);

    uv_loop_close(loop);
  } while (i-- != 0);

  remove("watch_dir/");
  MAKE_VALGRIND_HAPPY();
  return 0;
}

#else  /* !defined(__APPLE__) */

TEST_IMPL(fs_event_error_reporting) {
  /* No-op, needed only for FSEvents backend */

  MAKE_VALGRIND_HAPPY();
  return 0;
}

#endif  /* defined(__APPLE__) */

TEST_IMPL(fs_event_watch_invalid_path) {
#if defined(NO_FS_EVENTS)
  RETURN_SKIP(NO_FS_EVENTS);
#endif

  uv_loop_t* loop;
  int r;

  loop = uv_default_loop();
  r = uv_fs_event_init(loop, &fs_event);
  ASSERT(r == 0);
  r = uv_fs_event_start(&fs_event, fs_event_cb_file, "<:;", 0);
  ASSERT(r != 0);
  ASSERT(uv_is_active((uv_handle_t*) &fs_event) == 0);
  r = uv_fs_event_start(&fs_event, fs_event_cb_file, "", 0);
  ASSERT(r != 0);
  ASSERT(uv_is_active((uv_handle_t*) &fs_event) == 0);
  MAKE_VALGRIND_HAPPY();
  return 0;
}

static int fs_event_cb_stop_calls;

static void fs_event_cb_stop(uv_fs_event_t* handle, const char* path,
                             int events, int status) {
  uv_fs_event_stop(handle);
  fs_event_cb_stop_calls++;
}

TEST_IMPL(fs_event_stop_in_cb) {
  uv_fs_event_t fs;
  uv_timer_t timer;
  char path[] = "fs_event_stop_in_cb.txt";

#if defined(NO_FS_EVENTS)
  RETURN_SKIP(NO_FS_EVENTS);
#endif

  remove(path);
  create_file(path);

  ASSERT_EQ(0, uv_fs_event_init(uv_default_loop(), &fs));
  ASSERT_EQ(0, uv_fs_event_start(&fs, fs_event_cb_stop, path, 0));

  /* Note: timer_cb_touch() closes the handle. */
  timer.data = path;
  ASSERT_EQ(0, uv_timer_init(uv_default_loop(), &timer));
  ASSERT_EQ(0, uv_timer_start(&timer, timer_cb_touch, 100, 0));

  ASSERT_EQ(0, fs_event_cb_stop_calls);
  ASSERT_EQ(0, timer_cb_touch_called);

  ASSERT_EQ(0, uv_run(uv_default_loop(), UV_RUN_DEFAULT));

  ASSERT_EQ(1, fs_event_cb_stop_calls);
  ASSERT_EQ(1, timer_cb_touch_called);

  uv_close((uv_handle_t*) &fs, NULL);
  ASSERT_EQ(0, uv_run(uv_default_loop(), UV_RUN_DEFAULT));
  ASSERT_EQ(1, fs_event_cb_stop_calls);

  remove(path);

  MAKE_VALGRIND_HAPPY();
  return 0;
}
