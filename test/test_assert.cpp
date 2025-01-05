#include <corio/detail/assert.hpp>
#include <doctest/doctest.h>

namespace {

void assert_fail1() { CORIO_ASSERT(false, "this should fail"); }

void assert_fail2() { CORIO_ASSERT(1 == 2, "this should fail"); }

void assert_fail3() { CORIO_ASSERT(1 == 2); }

void assert_fail4() { CORIO_ASSERT(false); }

void assert_success() { CORIO_ASSERT(true); }

} // namespace

#ifndef NDEBUG

TEST_CASE("test assert") {
    CHECK_THROWS_AS(assert_fail1(), corio::AssertionError);
    CHECK_THROWS_AS(assert_fail2(), corio::AssertionError);
    CHECK_THROWS_AS(assert_fail3(), corio::AssertionError);
    CHECK_THROWS_AS(assert_fail4(), corio::AssertionError);
    CHECK_NOTHROW(assert_success());
}

#endif