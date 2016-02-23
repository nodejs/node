/*
 *  libfaketime wrapper command
 *
 *  This file is part of libfaketime, version 0.9.6
 *
 *  libfaketime is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License v2 as published by the
 *  Free Software Foundation.
 *
 *  libfaketime is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 *  more details.
 *
 *  You should have received a copy of the GNU General Public License v2 along
 *  with the libfaketime; if not, write to the Free Software Foundation,
 *  Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Converted from shell script by Balint Reczey with the following credits
 * and comments:
 *
 * Thanks to Daniel Kahn Gillmor for improvement suggestions.

 * This wrapper exposes only a small subset of the libfaketime functionality.
 * Please see libfaketime's README file and man page for more details.

 * Acknowledgment: Parts of the functionality of this wrapper have been
 * inspired by Matthias Urlichs' datefudge 1.14.

 * Compile time configuration: Path where the libfaketime libraries can be found
 * on Linux/UNIX
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <semaphore.h>

#include "faketime_common.h"

const char version[] = "0.9.6";

#ifdef __APPLE__
static const char *date_cmd = "gdate";
#else
static const char *date_cmd = "date";
#endif

#define PATH_BUFSIZE 4096

/* semaphore and shared memory names */
char sem_name[PATH_BUFSIZE] = {0}, shm_name[PATH_BUFSIZE] = {0};

void usage(const char *name)
{
  printf("\n");
  printf("Usage: %s [switches] <timestamp> <program with arguments>\n", name);
  printf("\n");
  printf("This will run the specified 'program' with the given 'arguments'.\n");
  printf("The program will be tricked into seeing the given 'timestamp' as its starting date and time.\n");
  printf("The clock will continue to run from this timestamp. Please see the manpage (man faketime)\n");
  printf("for advanced options, such as stopping the wall clock and make it run faster or slower.\n");
  printf("\n");
  printf("The optional switches are:\n");
  printf("  -m                  : Use the multi-threaded version of libfaketime\n");
  printf("  -f                  : Use the advanced timestamp specification format (see manpage)\n");
  printf("  --exclude-monotonic : Prevent monotonic clock from drifting (not the raw monotonic one)\n");
  printf("\n");
  printf("Examples:\n");
  printf("%s 'last friday 5 pm' /bin/date\n", name);
  printf("%s '2008-12-24 08:15:42' /bin/date\n", name);
  printf("%s -f '+2,5y x10,0' /bin/bash -c 'date; while true; do echo $SECONDS ; sleep 1 ; done'\n", name);
  printf("%s -f '+2,5y x0,50' /bin/bash -c 'date; while true; do echo $SECONDS ; sleep 1 ; done'\n", name);
  printf("%s -f '+2,5y i2,0' /bin/bash -c 'date; while true; do date; sleep 1 ; done'\n", name);
  printf("In this single case all spawned processes will use the same global clock\n");
  printf("without restaring it at the start of each process.\n\n");
  printf("(Please note that it depends on your locale settings whether . or , has to be used for fractions)\n");
  printf("\n");
}

/** Clean up shared objects */
static void cleanup_shobjs()
{
  if (-1 == sem_unlink(sem_name))
  {
    perror("sem_unlink");
  }
  if (-1 == shm_unlink(shm_name))
  {
    perror("shm_unlink");
  }
}

int main (int argc, char **argv)
{
  pid_t child_pid;
  int curr_opt = 1;
  bool use_mt = false, use_direct = false;
  long offset;

  while(curr_opt < argc)
  {
    if (0 == strcmp(argv[curr_opt], "-m"))
    {
      use_mt = true;
      curr_opt++;
      continue;
    }
    else if (0 == strcmp(argv[curr_opt], "-f"))
    {
      use_direct = true;
      curr_opt++;
      continue;
    }
    else if (0 == strcmp(argv[curr_opt], "--exclude-monotonic"))
    {
      setenv("DONT_FAKE_MONOTONIC", "1", true);
      curr_opt++;
      continue;
    }
    else if ((0 == strcmp(argv[curr_opt], "-v")) ||
             (0 == strcmp(argv[curr_opt], "--version")))
    {
      printf("\n%s: Version %s\n"
         "For usage information please use '%s --help'.\n",
         argv[0], version, argv[0]);
      exit(EXIT_SUCCESS);
    }
    else if ((0 == strcmp(argv[curr_opt], "-h")) ||
             (0 == strcmp(argv[curr_opt], "-?")) ||
             (0 == strcmp(argv[curr_opt], "--help")))
    {
      usage(argv[0]);
      exit(EXIT_SUCCESS);
    }
    else
    {
      /* we parsed all options */
      break;
    }
  }

  /* we need at least a timestamp string and a command to run */
  if (argc - curr_opt < 2)
  {
    usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  if (!use_direct)
  {
    // TODO get seconds
    int pfds[2];
    (void) (pipe(pfds) + 1);
    int ret = EXIT_SUCCESS;

    if (0 == (child_pid = fork()))
    {
      close(1);       /* close normal stdout */
      (void) (dup(pfds[1]) + 1);   /* make stdout same as pfds[1] */
      close(pfds[0]); /* we don't need this */
      if (EXIT_SUCCESS != execlp(date_cmd, date_cmd, "-d", argv[curr_opt], "+%s",(char *) NULL))
      {
        perror("Running (g)date failed");
        exit(EXIT_FAILURE);
      }
    }
    else
    {
      char buf[256] = {0}; /* e will have way less than 256 digits */
      close(pfds[1]);   /* we won't write to this */
      (void) (read(pfds[0], buf, 256) + 1);
      waitpid(child_pid, &ret, 0);
      if (ret != EXIT_SUCCESS)
      {
        printf("Error: Timestamp to fake not recognized, please re-try with a "
               "different timestamp.\n");
        exit(EXIT_FAILURE);
      }
      offset = atol(buf) - time(NULL);
      ret = snprintf(buf, sizeof(buf), "%s%ld", (offset >= 0)?"+":"", offset);
      setenv("FAKETIME", buf, true);
      close(pfds[0]); /* finished reading */
    }
  }
  else
  {
    /* simply pass format string along */
    setenv("FAKETIME", argv[curr_opt], true);
  }
  int keepalive_fds[2];
  (void) (pipe(keepalive_fds) + 1);

  /* we just consumed the timestamp option */
  curr_opt++;

  {
    /* create semaphores and shared memory */
    int shm_fd;
    sem_t *sem;
    struct ft_shared_s *ft_shared;
    char shared_objs[PATH_BUFSIZE];

    /*
     * Casting of getpid() return value to long needed to make GCC on SmartOS
     * happy, since getpid's return value's type on SmartOS is long. Since
     * getpid's return value's type is int on most other systems, and that
     * sizeof(long) always >= sizeof(int), this works on all platforms without
     * the need for crazy #ifdefs.
     */
    snprintf(sem_name, PATH_BUFSIZE -1 ,"/faketime_sem_%ld", (long)getpid());
    snprintf(shm_name, PATH_BUFSIZE -1 ,"/faketime_shm_%ld", (long)getpid());

    if (SEM_FAILED == (sem = sem_open(sem_name, O_CREAT|O_EXCL, S_IWUSR|S_IRUSR, 1)))
    {
      perror("sem_open");
      exit(EXIT_FAILURE);
    }

    /* create shm */
    if (-1 == (shm_fd = shm_open(shm_name, O_CREAT|O_EXCL|O_RDWR, S_IWUSR|S_IRUSR)))
    {
      perror("shm_open");
      if (-1 == sem_unlink(argv[2]))
      {
        perror("sem_unlink");
      }
      exit(EXIT_FAILURE);
    }

    /* set shm size */
    if (-1 == ftruncate(shm_fd, sizeof(uint64_t)))
    {
      perror("ftruncate");
      cleanup_shobjs();
      exit(EXIT_FAILURE);
    }

    /* map shm */
    if (MAP_FAILED == (ft_shared = mmap(NULL, sizeof(struct ft_shared_s), PROT_READ|PROT_WRITE,
                        MAP_SHARED, shm_fd, 0)))
    {
      perror("mmap");
      cleanup_shobjs();
      exit(EXIT_FAILURE);
    }

    if (sem_wait(sem) == -1)
    {
      perror("sem_wait");
      cleanup_shobjs();
      exit(EXIT_FAILURE);
    }

    /* init elapsed time ticks to zero */
    ft_shared->ticks = 0;
    ft_shared->file_idx = 0;
    ft_shared->start_time.real.tv_sec = 0;
    ft_shared->start_time.real.tv_nsec = -1;
    ft_shared->start_time.mon.tv_sec = 0;
    ft_shared->start_time.mon.tv_nsec = -1;
    ft_shared->start_time.mon_raw.tv_sec = 0;
    ft_shared->start_time.mon_raw.tv_nsec = -1;

    if (-1 == munmap(ft_shared, (sizeof(struct ft_shared_s))))
    {
      perror("munmap");
      cleanup_shobjs();
      exit(EXIT_FAILURE);
    }

    if (sem_post(sem) == -1)
    {
      perror("semop");
      cleanup_shobjs();
      exit(EXIT_FAILURE);
    }

    snprintf(shared_objs, PATH_BUFSIZE, "%s %s", sem_name, shm_name);
    setenv("FAKETIME_SHARED", shared_objs, true);
    sem_close(sem);
  }

  {
    char *ftpl_path;
#ifdef __APPLE__
    ftpl_path = PREFIX "/libfaketime.1.dylib";
    FILE *check;
    check = fopen(ftpl_path, "ro");
    if (check == NULL)
    {
      ftpl_path = PREFIX "/lib/faketime/libfaketime.1.dylib";
    }
    else
    {
      fclose(check);
    }
    setenv("DYLD_INSERT_LIBRARIES", ftpl_path, true);
    setenv("DYLD_FORCE_FLAT_NAMESPACE", "1", true);
#else
    {
      char *ld_preload_new, *ld_preload = getenv("LD_PRELOAD");
      size_t len;
      if (use_mt)
      {
        /*
         * on MultiArch platforms, such as Debian, we put a literal $LIB into LD_PRELOAD.
         */
#ifndef MULTI_ARCH
        ftpl_path = PREFIX LIBDIRNAME "/libfaketimeMT.so.1";
#else
        ftpl_path = PREFIX "/$LIB/faketime/libfaketimeMT.so.1";
#endif
      }
      else
      {
#ifndef MULTI_ARCH
        ftpl_path = PREFIX LIBDIRNAME "/libfaketime.so.1";
#else
        ftpl_path = PREFIX "/$LIB/faketime/libfaketime.so.1";
#endif
      }
      len = ((ld_preload)?strlen(ld_preload) + 1: 0) + 1 + strlen(ftpl_path);
      ld_preload_new = malloc(len);
      snprintf(ld_preload_new, len ,"%s%s%s", (ld_preload)?ld_preload:"",
              (ld_preload)?":":"", ftpl_path);
      setenv("LD_PRELOAD", ld_preload_new, true);
      free(ld_preload_new);
    }
#endif
  }

  /* run command and clean up shared objects */
  if (0 == (child_pid = fork()))
  {
    close(keepalive_fds[0]); /* only parent needs to read this */
    if (EXIT_SUCCESS != execvp(argv[curr_opt], &argv[curr_opt]))
    {
      perror("Running specified command failed");
      exit(EXIT_FAILURE);
    }
  }
  else
  {
    int ret;
    char buf;
    close(keepalive_fds[1]); /* only children need keep this open */
    waitpid(child_pid, &ret, 0);
    (void) (read(keepalive_fds[0], &buf, 1) + 1); /* reads 0B when all children exit */
    cleanup_shobjs();
    if (WIFSIGNALED(ret))
    {
      fprintf(stderr, "Caught %s\n", strsignal(WTERMSIG(ret)));
      exit(EXIT_FAILURE);
    }
    exit(WEXITSTATUS(ret));
  }

  return EXIT_SUCCESS;
}

/*
 * Editor modelines
 *
 * Local variables:
 * c-basic-offset: 2
 * tab-width: 2
 * indent-tabs-mode: nil
 * End:
 *
 * vi: set shiftwidth=2 tabstop=2 expandtab:
 * :indentSize=2:tabSize=2:noTabs=true:
 */

/* eof */
