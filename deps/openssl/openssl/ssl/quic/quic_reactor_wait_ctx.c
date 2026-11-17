/*
 * Copyright 2024-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */
#include "internal/quic_reactor_wait_ctx.h"
#include "internal/common.h"
#include "internal/thread_arch.h"
#include <assert.h>

struct quic_reactor_wait_slot_st {
    OSSL_LIST_MEMBER(quic_reactor_wait_slot, QUIC_REACTOR_WAIT_SLOT);
    QUIC_REACTOR *rtor; /* primary key */
    size_t blocking_count; /* datum */
    /* Generation captured at the 1->0 deregistration, for the phase-2 wait. */
    uint64_t notify_generation;
    unsigned int need_notify_wait : 1; /* set when phase 2 must wait */
};

DEFINE_LIST_OF_IMPL(quic_reactor_wait_slot, QUIC_REACTOR_WAIT_SLOT);

void ossl_quic_reactor_wait_ctx_init(QUIC_REACTOR_WAIT_CTX *ctx)
{
    ossl_list_quic_reactor_wait_slot_init(&ctx->slots);
}

static void slot_activate(QUIC_REACTOR_WAIT_SLOT *slot)
{
    if (++slot->blocking_count == 1)
        ossl_quic_reactor_enter_blocking_section(slot->rtor);
}

/*
 * Phase-1 deregistration: on the 1->0 transition, deregister from the reactor
 * without blocking and capture whether phase 2 must wait (and the generation).
 * Caller must hold the reactor mutex.
 */
static void slot_deactivate_deregister(QUIC_REACTOR_WAIT_SLOT *slot)
{
    assert(slot->blocking_count > 0);

    if (--slot->blocking_count > 0)
        return;

    slot->need_notify_wait
        = ossl_quic_reactor_deregister_blocking_section(slot->rtor,
              &slot->notify_generation)
        != 0;
}

int ossl_quic_reactor_wait_ctx_enter(QUIC_REACTOR_WAIT_CTX *ctx,
    QUIC_REACTOR *rtor)
{
    QUIC_REACTOR_WAIT_SLOT *slot;

    OSSL_LIST_FOREACH (slot, quic_reactor_wait_slot, &ctx->slots)
        if (slot->rtor == rtor)
            break;

    if (slot == NULL) {
        if ((slot = OPENSSL_zalloc(sizeof(QUIC_REACTOR_WAIT_SLOT))) == NULL)
            return 0;

        slot->rtor = rtor;
        ossl_list_quic_reactor_wait_slot_insert_tail(&ctx->slots, slot);
    }

    slot_activate(slot);
    return 1;
}

void ossl_quic_reactor_wait_ctx_deregister(QUIC_REACTOR_WAIT_CTX *ctx,
    QUIC_REACTOR *rtor)
{
    QUIC_REACTOR_WAIT_SLOT *slot;

    OSSL_LIST_FOREACH (slot, quic_reactor_wait_slot, &ctx->slots)
        if (slot->rtor == rtor)
            break;

    assert(slot != NULL);
    slot_deactivate_deregister(slot);
}

/*
 * Phase 2: wait for every notifier phase 1 left signalled to clear. Because
 * phase 1 has already decremented every slot's refcount to zero, the calling
 * thread holds no waiter registration here and cannot be part of a
 * cross-reactor deadlock. Each reactor's mutex is acquired internally.
 */
void ossl_quic_reactor_wait_ctx_drain(QUIC_REACTOR_WAIT_CTX *ctx)
{
    QUIC_REACTOR_WAIT_SLOT *slot;

    OSSL_LIST_FOREACH (slot, quic_reactor_wait_slot, &ctx->slots) {
        /* Invariant: phase 1 must have deregistered every slot by now. */
        assert(slot->blocking_count == 0);

        if (!slot->need_notify_wait)
            continue;

        ossl_quic_reactor_wait_for_notifier_clear(slot->rtor,
            slot->notify_generation);
        slot->need_notify_wait = 0;
    }
}

void ossl_quic_reactor_wait_ctx_cleanup(QUIC_REACTOR_WAIT_CTX *ctx)
{
    QUIC_REACTOR_WAIT_SLOT *slot, *nslot;

    OSSL_LIST_FOREACH_DELSAFE (slot, nslot, quic_reactor_wait_slot, &ctx->slots) {
        assert(slot->blocking_count == 0);
        assert(!slot->need_notify_wait);
        OPENSSL_free(slot);
    }
}
