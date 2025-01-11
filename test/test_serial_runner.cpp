#include <asio.hpp>
#include <corio/detail/serial_runner.hpp>
#include <doctest/doctest.h>

TEST_CASE("test serial runner") {

    SUBCASE("accept io context") {
        asio::io_context io_context;
        auto ex = io_context.get_executor();
        corio::detail::SerialRunner runner(ex);
        CHECK(runner.get_executor() == ex);
        CHECK(runner.get_inner_executor() == ex);

        auto runner2 = runner.fork_runner();
        CHECK(runner2.get_executor() == ex);
        CHECK(runner2.get_inner_executor() == ex);
    }

    SUBCASE("accept strand") {
        asio::io_context io_context;
        auto strand = asio::make_strand(io_context.get_executor());
        corio::detail::SerialRunner runner(strand);
        CHECK(runner.get_executor() == strand);
        CHECK(runner.get_inner_executor() == strand.get_inner_executor());

        auto runner2 = runner.fork_runner();
        CHECK(runner2.get_executor() != strand); // Another strand after fork
        CHECK(runner2.get_inner_executor() == runner.get_inner_executor());
    }

    SUBCASE("default constructor and assign") {
        corio::detail::SerialRunner runner;
        CHECK(!runner);

        asio::io_context io_context;
        auto ex = io_context.get_executor();
        runner = corio::detail::SerialRunner(ex);

        CHECK(runner.get_executor() == ex);
        CHECK(runner.get_inner_executor() == ex);

        runner = corio::detail::SerialRunner();
        CHECK(!runner);

        auto strand = asio::make_strand(ex);
        runner = corio::detail::SerialRunner(strand);
        CHECK(runner.get_executor() == strand);
        CHECK(runner.get_inner_executor() == strand.get_inner_executor());
    }
}