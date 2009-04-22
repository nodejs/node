#include <unistd.h> /* sleep() */
#include <stdlib.h> /* malloc(), free() */
#include <stdio.h> 
#include <errno.h> 
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <ev.h>
#include <oi_file.h>

static  oi_file file; 
static  oi_file out; 

#define READ_BUFSIZE (5)
static char read_buf[READ_BUFSIZE];
# define SEP "~~~~~~~~~~~~~~~~~~~~~~\n"

static void
on_open(oi_file *f)
{
  oi_file_write_simple(&out, "\n", 1);
  oi_file_write_simple(&out, SEP, sizeof(SEP));
  oi_file_read_start(f, read_buf, READ_BUFSIZE);
}

static void
on_close(oi_file *f)
{
  oi_file_write_simple(&out, SEP, sizeof(SEP));
  oi_file_detach(f);  
  out.on_drain = oi_file_detach;
}

static void
on_read(oi_file *f, size_t recved)
{
  if(recved == 0) /* EOF */
    oi_file_close(f);
  else
    oi_file_write_simple(&out, read_buf, recved);
}

int
main()
{
  struct ev_loop *loop = ev_default_loop(0);

  oi_file_init(&file);
  file.on_open = on_open;
  file.on_read = on_read;
  file.on_close = on_close;
  oi_file_open_path(&file, "LICENSE", O_RDONLY, 0);
  oi_file_attach(&file, loop);

  oi_file_init(&out);
  oi_file_open_stdout(&out);
  oi_file_attach(&out, loop);

  ev_loop(loop, 0);
  return 0;
}
