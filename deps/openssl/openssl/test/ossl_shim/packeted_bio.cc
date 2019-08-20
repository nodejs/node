/*
 * Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "packeted_bio.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <openssl/crypto.h>


namespace {

const uint8_t kOpcodePacket = 'P';
const uint8_t kOpcodeTimeout = 'T';
const uint8_t kOpcodeTimeoutAck = 't';

struct PacketedBio {
  explicit PacketedBio(bool advance_clock_arg)
      : advance_clock(advance_clock_arg) {
    memset(&timeout, 0, sizeof(timeout));
    memset(&clock, 0, sizeof(clock));
    memset(&read_deadline, 0, sizeof(read_deadline));
  }

  bool HasTimeout() const {
    return timeout.tv_sec != 0 || timeout.tv_usec != 0;
  }

  bool CanRead() const {
    if (read_deadline.tv_sec == 0 && read_deadline.tv_usec == 0) {
      return true;
    }

    if (clock.tv_sec == read_deadline.tv_sec) {
      return clock.tv_usec < read_deadline.tv_usec;
    }
    return clock.tv_sec < read_deadline.tv_sec;
  }

  timeval timeout;
  timeval clock;
  timeval read_deadline;
  bool advance_clock;
};

PacketedBio *GetData(BIO *bio) {
  return (PacketedBio *)BIO_get_data(bio);
}

const PacketedBio *GetData(const BIO *bio) {
  return GetData(const_cast<BIO*>(bio));
}

// ReadAll reads |len| bytes from |bio| into |out|. It returns 1 on success and
// 0 or -1 on error.
static int ReadAll(BIO *bio, uint8_t *out, size_t len) {
  while (len > 0) {
    int chunk_len = INT_MAX;
    if (len <= INT_MAX) {
      chunk_len = (int)len;
    }
    int ret = BIO_read(bio, out, chunk_len);
    if (ret <= 0) {
      return ret;
    }
    out += ret;
    len -= ret;
  }
  return 1;
}

static int PacketedWrite(BIO *bio, const char *in, int inl) {
  if (BIO_next(bio) == NULL) {
    return 0;
  }

  BIO_clear_retry_flags(bio);

  // Write the header.
  uint8_t header[5];
  header[0] = kOpcodePacket;
  header[1] = (inl >> 24) & 0xff;
  header[2] = (inl >> 16) & 0xff;
  header[3] = (inl >> 8) & 0xff;
  header[4] = inl & 0xff;
  int ret = BIO_write(BIO_next(bio), header, sizeof(header));
  if (ret <= 0) {
    BIO_copy_next_retry(bio);
    return ret;
  }

  // Write the buffer.
  ret = BIO_write(BIO_next(bio), in, inl);
  if (ret < 0 || (inl > 0 && ret == 0)) {
    BIO_copy_next_retry(bio);
    return ret;
  }
  assert(ret == inl);
  return ret;
}

static int PacketedRead(BIO *bio, char *out, int outl) {
  PacketedBio *data = GetData(bio);
  if (BIO_next(bio) == NULL) {
    return 0;
  }

  BIO_clear_retry_flags(bio);

  for (;;) {
    // Check if the read deadline has passed.
    if (!data->CanRead()) {
      BIO_set_retry_read(bio);
      return -1;
    }

    // Read the opcode.
    uint8_t opcode;
    int ret = ReadAll(BIO_next(bio), &opcode, sizeof(opcode));
    if (ret <= 0) {
      BIO_copy_next_retry(bio);
      return ret;
    }

    if (opcode == kOpcodeTimeout) {
      // The caller is required to advance any pending timeouts before
      // continuing.
      if (data->HasTimeout()) {
        fprintf(stderr, "Unprocessed timeout!\n");
        return -1;
      }

      // Process the timeout.
      uint8_t buf[8];
      ret = ReadAll(BIO_next(bio), buf, sizeof(buf));
      if (ret <= 0) {
        BIO_copy_next_retry(bio);
        return ret;
      }
      uint64_t timeout = (static_cast<uint64_t>(buf[0]) << 56) |
          (static_cast<uint64_t>(buf[1]) << 48) |
          (static_cast<uint64_t>(buf[2]) << 40) |
          (static_cast<uint64_t>(buf[3]) << 32) |
          (static_cast<uint64_t>(buf[4]) << 24) |
          (static_cast<uint64_t>(buf[5]) << 16) |
          (static_cast<uint64_t>(buf[6]) << 8) |
          static_cast<uint64_t>(buf[7]);
      timeout /= 1000;  // Convert nanoseconds to microseconds.

      data->timeout.tv_usec = timeout % 1000000;
      data->timeout.tv_sec = timeout / 1000000;

      // Send an ACK to the peer.
      ret = BIO_write(BIO_next(bio), &kOpcodeTimeoutAck, 1);
      if (ret <= 0) {
        return ret;
      }
      assert(ret == 1);

      if (!data->advance_clock) {
        // Signal to the caller to retry the read, after advancing the clock.
        BIO_set_retry_read(bio);
        return -1;
      }

      PacketedBioAdvanceClock(bio);
      continue;
    }

    if (opcode != kOpcodePacket) {
      fprintf(stderr, "Unknown opcode, %u\n", opcode);
      return -1;
    }

    // Read the length prefix.
    uint8_t len_bytes[4];
    ret = ReadAll(BIO_next(bio), len_bytes, sizeof(len_bytes));
    if (ret <= 0) {
      BIO_copy_next_retry(bio);
      return ret;
    }

    uint32_t len = (len_bytes[0] << 24) | (len_bytes[1] << 16) |
        (len_bytes[2] << 8) | len_bytes[3];
    uint8_t *buf = (uint8_t *)OPENSSL_malloc(len);
    if (buf == NULL) {
      return -1;
    }
    ret = ReadAll(BIO_next(bio), buf, len);
    if (ret <= 0) {
      fprintf(stderr, "Packeted BIO was truncated\n");
      return -1;
    }

    if (outl > (int)len) {
      outl = len;
    }
    memcpy(out, buf, outl);
    OPENSSL_free(buf);
    return outl;
  }
}

static long PacketedCtrl(BIO *bio, int cmd, long num, void *ptr) {
  if (cmd == BIO_CTRL_DGRAM_SET_NEXT_TIMEOUT) {
    memcpy(&GetData(bio)->read_deadline, ptr, sizeof(timeval));
    return 1;
  }

  if (BIO_next(bio) == NULL) {
    return 0;
  }
  BIO_clear_retry_flags(bio);
  int ret = BIO_ctrl(BIO_next(bio), cmd, num, ptr);
  BIO_copy_next_retry(bio);
  return ret;
}

static int PacketedNew(BIO *bio) {
  BIO_set_init(bio, 1);
  return 1;
}

static int PacketedFree(BIO *bio) {
  if (bio == NULL) {
    return 0;
  }

  delete GetData(bio);
  BIO_set_init(bio, 0);
  return 1;
}

static long PacketedCallbackCtrl(BIO *bio, int cmd, BIO_info_cb fp)
{
  if (BIO_next(bio) == NULL)
    return 0;
  return BIO_callback_ctrl(BIO_next(bio), cmd, fp);
}

static BIO_METHOD *g_packeted_bio_method = NULL;

static const BIO_METHOD *PacketedMethod(void)
{
  if (g_packeted_bio_method == NULL) {
    g_packeted_bio_method = BIO_meth_new(BIO_TYPE_FILTER, "packeted bio");
    if (   g_packeted_bio_method == NULL
        || !BIO_meth_set_write(g_packeted_bio_method, PacketedWrite)
        || !BIO_meth_set_read(g_packeted_bio_method, PacketedRead)
        || !BIO_meth_set_ctrl(g_packeted_bio_method, PacketedCtrl)
        || !BIO_meth_set_create(g_packeted_bio_method, PacketedNew)
        || !BIO_meth_set_destroy(g_packeted_bio_method, PacketedFree)
        || !BIO_meth_set_callback_ctrl(g_packeted_bio_method,
                                       PacketedCallbackCtrl))
    return NULL;
  }
  return g_packeted_bio_method;
}
}  // namespace

bssl::UniquePtr<BIO> PacketedBioCreate(bool advance_clock) {
  bssl::UniquePtr<BIO> bio(BIO_new(PacketedMethod()));
  if (!bio) {
    return nullptr;
  }
  BIO_set_data(bio.get(), new PacketedBio(advance_clock));
  return bio;
}

timeval PacketedBioGetClock(const BIO *bio) {
  return GetData(bio)->clock;
}

bool PacketedBioAdvanceClock(BIO *bio) {
  PacketedBio *data = GetData(bio);
  if (data == nullptr) {
    return false;
  }

  if (!data->HasTimeout()) {
    return false;
  }

  data->clock.tv_usec += data->timeout.tv_usec;
  data->clock.tv_sec += data->clock.tv_usec / 1000000;
  data->clock.tv_usec %= 1000000;
  data->clock.tv_sec += data->timeout.tv_sec;
  memset(&data->timeout, 0, sizeof(data->timeout));
  return true;
}
