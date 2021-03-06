#ifndef __UVWASI_POLL_ONEOFF_H__
#define __UVWASI_POLL_ONEOFF_H__

#include "fd_table.h"
#include "wasi_types.h"

struct uvwasi_s;

struct uvwasi__poll_fdevent_t {
  struct uvwasi_fd_wrap_t* wrap;
  uvwasi_userdata_t userdata;
  uvwasi_eventtype_t type;
  uvwasi_errno_t error;
  uv_poll_t* poll_handle;
  int is_duplicate_fd;
  int events;
  int revents;
};

struct uvwasi_poll_oneoff_state_t {
  struct uvwasi_s* uvwasi;
  struct uvwasi__poll_fdevent_t* fdevents;
  uv_poll_t* poll_handles;
  uv_timer_t timer;
  uint64_t timeout;
  uv_loop_t loop;
  uvwasi_size_t max_fds;
  int has_timer;
  uvwasi_size_t fdevent_cnt;
  uvwasi_size_t handle_cnt;
  int result;
};


uvwasi_errno_t uvwasi__poll_oneoff_state_init(
                                      struct uvwasi_s* uvwasi,
                                      struct uvwasi_poll_oneoff_state_t* state,
                                      uvwasi_size_t max_fds
                                    );

uvwasi_errno_t uvwasi__poll_oneoff_state_cleanup(
                                        struct uvwasi_poll_oneoff_state_t* state
                                      );

uvwasi_errno_t uvwasi__poll_oneoff_state_set_timer(
                                      struct uvwasi_poll_oneoff_state_t* state,
                                      uvwasi_timestamp_t timeout
                                    );

uvwasi_errno_t uvwasi__poll_oneoff_state_add_fdevent(
                                      struct uvwasi_poll_oneoff_state_t* state,
                                      uvwasi_subscription_t* subscription
                                    );

uvwasi_errno_t uvwasi__poll_oneoff_run(
                                      struct uvwasi_poll_oneoff_state_t* state
                                    );


#endif /* __UVWASI_POLL_ONEOFF_H__ */
