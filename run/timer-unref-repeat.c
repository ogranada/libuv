/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "uv.h"
#include "../test/task.h"
#include <stdlib.h>


static uint64_t start_time;

static uv_timer_t repeat_timer;
static unsigned int unref_interval_cb_called = 0;
static unsigned int interval_cb_called = 0;

static void eat_time() {
  for (int i = 0; i < 1E7; ++i) rand();
}

static void unref_interval_cb(uv_timer_t* handle) {
  ASSERT(handle != NULL);
  unref_interval_cb_called++;

  uv_unref((uv_handle_t*)handle);
  eat_time();
}

/*
 * Reproduces the following scenario:
 *
 * ```js
 *  var callbacks = 0;
 *
 *  // callbacks get scheduled faster than eatTime can process them
 *  var i = setInterval(function () {
 *    callbacks++
 *    i.unref()
 *    eatTime()
 *  }, 10)
 *
 *  function eatTime() {
 *    // the goal of this function is to take longer than the interval
 *    var count = 0;
 *    while (count++ < 1E7) {
 *      Math.random();
 *    }
 *  }
 *
 *  process.on("exit", function () {
 *    assert.equal(callbacks, 1);
 *  });
 * ```
 */

static void run_unref_in_cb(uv_run_mode mode,
                unsigned int timeout,
                unsigned int expect_more,
                unsigned int expect_call_count) {
  int r;
  unsigned int more;
  unref_interval_cb_called = 0;

  start_time = uv_now(uv_default_loop());
  ASSERT(0 < start_time);

  r = uv_timer_init(uv_default_loop(), &repeat_timer);
  ASSERT(r == 0);

  r = uv_timer_start(&repeat_timer, unref_interval_cb, timeout, 1);
  ASSERT(r == 0);

  more = uv_run(uv_default_loop(), mode);
  ASSERT(more == expect_more);

  ASSERT(unref_interval_cb_called == expect_call_count);

  MAKE_VALGRIND_HAPPY();
}

TEST_IMPL(timer_unref_once) {
  unsigned int expect_call_count = 1;
  unsigned int expect_more = 0;
  run_unref_in_cb(UV_RUN_ONCE,10, expect_more, expect_call_count);
  run_unref_in_cb(UV_RUN_ONCE,1, expect_more, expect_call_count);
  return 0;
}

// NOWAIT doesn't block if no callbacks are pending
// more will be set since our timer handle will still be hanging around
TEST_IMPL(timer_unref_nowait) {
  unsigned int expect_call_count = 0;
  unsigned int expect_more = 1;
  run_unref_in_cb(UV_RUN_NOWAIT, 10, expect_more, expect_call_count);
  run_unref_in_cb(UV_RUN_NOWAIT, 1, expect_more, expect_call_count);
  return 0;
}

TEST_IMPL(timer_unref_default) {
  unsigned int expect_call_count = 1;
  unsigned int expect_more = 0;
  run_unref_in_cb(UV_RUN_DEFAULT, 10, expect_more, expect_call_count);
  run_unref_in_cb(UV_RUN_DEFAULT, 1, expect_more, expect_call_count);
  return 0;
}

int main(void) {
  run_test_timer_unref_once();
  run_test_timer_unref_nowait();
  run_test_timer_unref_default();
  return 0;
}
