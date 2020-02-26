/*
 * This file is part of the Trezor project, https://trezor.io/
 *
 * Copyright (c) SatoshiLabs
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdbool.h>
#include <stdint.h>

#include "memzero.h"
#include "rand.h"
#include "rdi.h"

#define BUFFER_LENGTH 64
#define RESEED_INTERVAL 65536

static CHACHA_DRBG_CTX drbg_ctx;
static uint8_t buffer[BUFFER_LENGTH];
static size_t buffer_index;
static uint8_t session_delay;
static bool rdi_enabled;

static void rdi_reseed(void) {
  uint8_t entropy[CHACHA_DRBG_SEED_LENGTH];
  random_buffer(entropy, CHACHA_DRBG_SEED_LENGTH);
  chacha_drbg_reseed(&drbg_ctx, entropy);
}

void buffer_refill(void) {
  chacha_drbg_generate(&drbg_ctx, buffer, BUFFER_LENGTH);
}

static uint32_t random8(void) {
  buffer_index += 1;
  if (buffer_index >= BUFFER_LENGTH) {
    buffer_refill();
    if (RESEED_INTERVAL != 0 && drbg_ctx.reseed_counter > RESEED_INTERVAL)
      rdi_reseed();
    buffer_index = 0;
  }
  return buffer[buffer_index];
}

void rdi_regenerate_session_delay(void) {
  if (rdi_enabled) session_delay = random8();
}

void rdi_handler(uint32_t uw_tick) {
  if (rdi_enabled) {
    uint32_t delay = random8() + session_delay;

    asm volatile(
        "ldr r0, %0;"  // r0 = delay
        "add r0, $3;"  // r0 += 3
        "loop:"
        "subs r0, $3;"  // r0 -= 3
        "bhs loop;"     // if (r0 >= 3): goto loop
        // loop ((delay // 3) + 1) times
        // one extra loop ensures that branch predictor learns the loop
        // every loop takes 3 ticks
        // r0 == (delay % 3) - 3
        "lsl r0, $1;"      // r0 *= 2
        "add r0, $4;"      // r0 += 4
        "rsb r0, r0, $0;"  // r0 = -r0
        // r0 = 2 if (delay % 3 == 0) else 0 if (delay % 3 == 1) else -2 if
        // (delay % 3 == 2)
        "add pc, r0;"  // jump (r0 + 2)/2 instructions ahead
        // jump here if (delay % 3 == 2)
        "nop;"  // wait one tick
        // jump here if (delay % 3 == 1)
        "nop;"  // wait one tick
        // jump here if (delay % 3 == 0)
        :
        : "m"(delay)
        : "r0");  // wait (24 + delay) ticks
  }
}

void rdi_start(void) {
  if (!rdi_enabled) {
    uint8_t entropy[CHACHA_DRBG_SEED_LENGTH];
    random_buffer(entropy, CHACHA_DRBG_SEED_LENGTH);
    chacha_drbg_init(&drbg_ctx, entropy);
    buffer_refill();
    buffer_index = 0;
    session_delay = random8();
    rdi_enabled = true;
  }
}

void rdi_stop(void) {
  if (rdi_enabled) {
    rdi_enabled = false;
    session_delay = 0;
    memzero(&drbg_ctx, sizeof(drbg_ctx));
  }
}
