#include <unistd.h> /* sleep() */
#include <stdlib.h> /* malloc(), free() */
#include <assert.h>
#include <ev.h>
#include <oi_async.h>

#define SLEEPS 4
static int runs = 0;

static void
done (oi_task *task, unsigned int result)
{
  assert(result == 0);
  if(++runs == SLEEPS) {
    ev_timer *timer = task->data;
    ev_timer_stop(task->async->loop, timer);
    oi_async_detach(task->async);
  }
  free(task);
}

static void
on_timeout(struct ev_loop *loop, ev_timer *w, int events)
{
  assert(0 && "timeout before all sleeping tasks were complete!");
}

int
main()
{
  struct ev_loop *loop = ev_default_loop(0);
  oi_async async;
  ev_timer timer;
  int i;

  oi_async_init(&async);
  for(i = 0; i < SLEEPS; i++) {
    oi_task *task = malloc(sizeof(oi_task));
    oi_task_init_sleep(task, done, 1);
    task->data = &timer;
    oi_async_submit(&async, task);
  }
  oi_async_attach(loop, &async);

  
  ev_timer_init (&timer, on_timeout, 1.2, 0.);
  ev_timer_start (loop, &timer);

  ev_loop(loop, 0);

  assert(runs == SLEEPS);

  return 0;
}
