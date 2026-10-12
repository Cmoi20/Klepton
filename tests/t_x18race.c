// The `br x18` veneer's race windows, MANUFACTURED and executed — `make x18race`,
// in `make check`.
//
// The kernel zeroes x18 on every exception return, so an interrupt that lands
// inside a veneer which holds a value in x18 loses it. Where that happens at
// random, in a guest thread, on a headset, a fix for it cannot be verified
// where it matters. So the race is manufactured here instead: entering the
// veneer's tail with x18 already zero produces a state indistinguishable from
// the real thing, and the code under test either survives it or this process
// is gone.
//
// No guest, no headset; milliseconds.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../runtime/klepton.h"     // kl_thread_init — seeds the TSD slot
#include "../runtime/kl_x18.h"

static int g_fail;
static void ck(int ok, const char *what) {
    if (!ok) g_fail++;
    printf("  %-4s %s\n", ok ? "ok" : "FAIL", what);
}

// The `ldr`/`br` window and the `cbz` guard in front of it.
//
// An interrupt landing on the veneer's `br x18` branches to address 0, which on
// visionOS is an unblockable SIGKILL: no signal is delivered and no handler can
// see it. So the veneer carries a `cbz` between the load and the branch, and
// this probe enters where the kernel leaves execution — at the `cbz`, with
// x18 already zeroed — and must come back with the slot's value rather than
// branching to 0.
//
//   b   9f                    <- jump to the manufactured entry
// 0:mrs x18, tpidrro_el0
//   ldr x18, [x18, #4000]
// 2:cbz x18, 0b               <- the guard
//   mov x0, x18
//   ret
// 9:mov x18, xzr              <- stands in for the exception return
//   b   2b
//
// Written as assembly rather than hand-encoded words because the point is that
// the shape matches what kl_x18.c emits. 4000 is KLX_TSD_SLOT * 8 and is
// checked against it below. Without the guard the instruction after the `cbz`
// slot is `br x18` with x18 zero, and this process is gone with nothing
// written down.
__attribute__((naked, noinline)) static uint64_t x18_br_guard_probe(void) {
    __asm__ volatile(
        "b   9f\n"
        "0:\n"
        "mrs x18, tpidrro_el0\n"
        "ldr x18, [x18, #4000]\n"
        "2:\n"
        "cbz x18, 0b\n"
        "mov x0, x18\n"
        "ret\n"
        "9:\n"
        "mov x18, xzr\n"
        "b   2b\n");
}

int main(void) {
    printf("=== x18 veneer race windows ===\n");
    ck(KLX_TSD_SLOT * 8 == 4000,
       "the probe's #4000 is KLX_TSD_SLOT*8 (the shadow slot's offset)");

    // The veneers load the guest's shadow x18 out of the TSD slot, so the slot
    // has to exist before anything can be reloaded from it.
    kl_thread_init();

    // The slot has to hold a real target first. kl_thread_init leaves this
    // thread's shadow x18 at zero — no guest code has run — and a zero slot is
    // the one case the guard deliberately SPINS on rather than branching to 0
    // (a guest asking to branch to address 0 is a guest bug; a live process
    // with a stack beats an unblockable SIGKILL). Seeding it is therefore part
    // of manufacturing the race, not a workaround: without it this test hangs,
    // which is itself the documented behaviour.
    uint64_t tsd_base;
    __asm__ volatile("mrs %0, tpidrro_el0" : "=r"(tsd_base));
    uint64_t *slot = &((uint64_t *)tsd_base)[KLX_TSD_SLOT];
    uint64_t saved = *slot;
    *slot = 0xdeadbeef00;
    uint64_t g = x18_br_guard_probe();
    *slot = saved;
    ck(g == 0xdeadbeef00, "the guard reloaded the slot's value after the zeroing");
    ck(g != 0, "...instead of letting `br x18` reach address 0");
    printf("       shadow x18 after the guard: 0x%llx\n", (unsigned long long)g);

    printf(g_fail ? "=== FAILED (%d) ===\n" : "=== x18 veneer race windows OK ===\n", g_fail);
    return g_fail ? 1 : 0;
}
