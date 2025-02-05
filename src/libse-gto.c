/*****************************************************************************
 * Copyright ©2017-2019 Gemalto – a Thales Company. All rights Reserved.
 *
 * This copy is licensed under the Apache License, Version 2.0 (the "License");
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *     http://www.apache.org/licenses/LICENSE-2.0 or https://www.apache.org/licenses/LICENSE-2.0.html 
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and limitations under the License.

 ****************************************************************************/

/**
 * @file
 * $Author$
 * $Revision$
 * $Date$
 *
 * libse-gto main functions.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <fcntl.h>

#include "se-gto/libse-gto.h"
#include "libse-gto-private.h"
#include "spi.h"

#include "compiler.h"

SE_GTO_EXPORT void *
se_gto_get_userdata(struct se_gto_ctx *ctx)
{
    if (ctx == NULL)
        return NULL;

    return ctx->userdata;
}

SE_GTO_EXPORT void
se_gto_set_userdata(struct se_gto_ctx *ctx, void *userdata)
{
    if (ctx == NULL)
        return;

    ctx->userdata = userdata;
}

static int
log_level(const char *priority)
{
    char *endptr;
    int   prio;

    prio = strtol(priority, &endptr, 10);
    if ((endptr[0] == '\0') || isspace(endptr[0]))
        return prio;

    if (strncmp(priority, "err", 3) == 0)
        return 0;

    if (strncmp(priority, "info", 4) == 0)
        return 3;

    if (strncmp(priority, "debug", 5) == 0)
        return 4;

    return 0;
}

static void
log_stderr(struct se_gto_ctx *ctx, const char *s)
{
    fputs(s, stderr);
}

SE_GTO_EXPORT int
se_gto_new(struct se_gto_ctx **c)
{
    const char        *env;
    struct se_gto_ctx *ctx;

    ctx = calloc(1, sizeof(struct se_gto_ctx));
    if (!ctx) {
        errno = ENOMEM;
        return -1;
    }

    isot1_init(&ctx->t1);

    ctx->log_fn = log_stderr;

    ctx->gtodev = SE_GTO_GTODEV;

    ctx->log_level = 2;
    /* environment overwrites config */
    env = getenv("SE_GTO_LOG");
    if (env != NULL)
        se_gto_set_log_level(ctx, log_level(env));

    dbg("ctx %p created\n", ctx);
    dbg("log_level=%d\n", ctx->log_level);
    *c = ctx;
    return 0;
}

SE_GTO_EXPORT int
se_gto_get_log_level(struct se_gto_ctx *ctx)
{
    return ctx->log_level;
}

SE_GTO_EXPORT void
se_gto_set_log_level(struct se_gto_ctx *ctx, int level)
{
    if (level < 0)
        level = 0;
    else if (level > 4)
        level = 4;
    ctx->log_level = level;
}

SE_GTO_EXPORT se_gto_log_fn *
se_gto_get_log_fn(struct se_gto_ctx *ctx)
{
    return ctx->log_fn;
}

SE_GTO_EXPORT void
se_gto_set_log_fn(struct se_gto_ctx *ctx, se_gto_log_fn *fn)
{
    ctx->log_fn = fn;
}

SE_GTO_EXPORT const char *
se_gto_get_gtodev(struct se_gto_ctx *ctx)
{
    return ctx->gtodev;
}

SE_GTO_EXPORT void
se_gto_set_gtodev(struct se_gto_ctx *ctx, const char *gtodev)
{
    ctx->gtodev = strdup(gtodev);
}

SE_GTO_EXPORT int
se_gto_reset(struct se_gto_ctx *ctx, void *atr, size_t r)
{
    int err;

    err = isot1_reset(&ctx->t1);
    if (err < 0)
        errno = -err;
    else {
        err = isot1_get_atr(&ctx->t1, atr, r);
        if (err < 0)
            errno = -err;
    }
    return err;
}

SE_GTO_EXPORT int
se_gto_apdu_transmit(struct se_gto_ctx *ctx, const void *apdu, int n, void *resp, int r)
{
    if (!apdu || (n < 4) || !resp || (r < 2)) {
        errno = EINVAL;
        return -1;
    }
    r = isot1_transceive(&ctx->t1, apdu, n, resp, r);
    if (r < 0) {
        errno = -r;
        err("failed to read APDU response, %s\n", strerror(-r));
        return -1;
    } else if (r < 2) {
        err("APDU response too short, only %d bytes, needs 2 at least\n", r);
        return -1;
    }
    return r;
}

SE_GTO_EXPORT int
se_gto_open(struct se_gto_ctx *ctx)
{
    info("eSE GTO: using %s\n", ctx->gtodev);

    if (spi_setup(ctx) < 0) {
        err("failed to set up se-gto.\n");
        return -1;
    }
    isot1_bind(&ctx->t1, 0x2, 0x1);

    dbg("fd: spi=%d\n", ctx->t1.spi_fd);
    return 0;
}

SE_GTO_EXPORT int
se_gto_close(struct se_gto_ctx *ctx)
{
    (void)isot1_release(&ctx->t1);
    (void)spi_teardown(ctx);
    log_teardown(ctx);
    free(ctx);
    return 0;
}
