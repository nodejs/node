/*
 * Faketime's common definitions
 *
 * Copyright 2013 Balint Reczey <balint@balintreczey.hu>
 *
 * This file is part of the libfaketime.
 *
 * libfaketime is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License v2 as published by the Free
 * Software Foundation.
 *
 * libfaketime is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License v2 along
 * with libfaketime; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef FAKETIME_COMMON_H
#define FAKETIME_COMMON_H

#include <stdint.h>

struct system_time_s
{
  /* System time according to CLOCK_REALTIME */
  struct timespec real;
  /* System time according to CLOCK_MONOTONIC */
  struct timespec mon;
  /* System time according to CLOCK_MONOTONIC_RAW */
  struct timespec mon_raw;
};

/* Data shared among faketime-spawned processes */
struct ft_shared_s
{
  /*
   * When advancing time linearly with each time(), etc. call, the calls are
   * counted here */
  uint64_t ticks;
  /* Index of timstamp to be loaded from file */
  uint64_t file_idx;
  /* System time Faketime started at */
  struct system_time_s start_time;
};

#endif
