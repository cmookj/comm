#include <gtest/gtest.h>

#include <limits>
#include <random>  // For QuickCheck
#include <type_traits>

#include "core/comm_message.h"

constexpr size_t max_test_count = 65536;

template <typename T> struct random_int_gen {
    random_int_gen () {
        // Create a random device to seed the generator
        std::random_device rd;

        // Use the Mersenne Twister engine
        gen = std::mt19937 (rd());

        // Define distributions
        if constexpr (std::is_unsigned_v<T>)
            dist = std::uniform_int_distribution<T> (0, std::numeric_limits<T>::max());
        else
            dist = std::uniform_int_distribution<T> (
                std::numeric_limits<T>::min(), std::numeric_limits<T>::max()
            );
    }

    T
    operator() () {
        return dist (gen);
    }

    std::mt19937                     gen;
    std::uniform_int_distribution<T> dist;
};

template <typename T> struct random_float_gen {
    random_float_gen () {
        // Create a random device to seed the generator
        std::random_device rd;

        // Use the Mersenne Twister engine
        gen = std::mt19937 (rd());

        // Define distributions
        if constexpr (!std::is_floating_point_v<T>)
            throw std::runtime_error ("The template parameter should be floating_point");

        dist = std::uniform_real_distribution<T> (0., 1.);
    }

    T
    operator() () {
        return dist (gen);
    }

    std::mt19937                      gen;
    std::uniform_real_distribution<T> dist;
};

TEST (MessageTest, UInt8Test) {
    gpw::net::message<uint16_t> msg;
    random_int_gen<uint8_t>     gen;

    for (size_t i = 0; i < max_test_count; ++i) {
        auto input = gen();

        msg << input;

        uint8_t output;
        msg >> output;

        EXPECT_EQ (input, output);
    }
}

TEST (MessageTest, Int8Test) {
    gpw::net::message<uint16_t> msg;
    random_int_gen<int8_t>      gen;

    for (size_t i = 0; i < max_test_count; ++i) {
        auto input = gen();

        msg << input;

        int8_t output;
        msg >> output;

        EXPECT_EQ (input, output);
    }
}

TEST (MessageTest, UInt16Test) {
    gpw::net::message<uint16_t> msg;
    random_int_gen<uint16_t>    gen;

    for (size_t i = 0; i < max_test_count; ++i) {
        auto input = gen();

        msg << input;

        uint16_t output;
        msg >> output;

        EXPECT_EQ (input, output);
    }
}

TEST (MessageTest, Int16Test) {
    gpw::net::message<uint16_t> msg;
    random_int_gen<int16_t>     gen;

    for (size_t i = 0; i < max_test_count; ++i) {
        auto input = gen();

        msg << input;

        int16_t output;
        msg >> output;

        EXPECT_EQ (input, output);
    }
}

TEST (MessageTest, UInt32Test) {
    gpw::net::message<uint16_t> msg;
    random_int_gen<uint32_t>    gen;

    for (size_t i = 0; i < max_test_count; ++i) {
        auto input = gen();

        msg << input;

        uint32_t output;
        msg >> output;

        EXPECT_EQ (input, output);
    }
}

TEST (MessageTest, Int32Test) {
    gpw::net::message<uint16_t> msg;
    random_int_gen<int32_t>     gen;

    for (size_t i = 0; i < max_test_count; ++i) {
        auto input = gen();

        msg << input;

        int32_t output;
        msg >> output;

        EXPECT_EQ (input, output);
    }
}

TEST (MessageTest, UInt64Test) {
    gpw::net::message<uint16_t> msg;
    random_int_gen<uint64_t>    gen;

    for (size_t i = 0; i < max_test_count; ++i) {
        auto input = gen();

        msg << input;

        uint64_t output;
        msg >> output;

        EXPECT_EQ (input, output);
    }
}

TEST (MessageTest, Int64Test) {
    gpw::net::message<uint16_t> msg;
    random_int_gen<int64_t>     gen;

    for (size_t i = 0; i < max_test_count; ++i) {
        auto input = gen();

        msg << input;

        int64_t output;
        msg >> output;

        EXPECT_EQ (input, output);
    }
}

TEST (MessageTest, FloatTest) {
    gpw::net::message<uint16_t> msg;
    random_float_gen<float>     gen;

    for (size_t i = 0; i < max_test_count; ++i) {
        auto input = gen();

        msg << input;

        float output;
        msg >> output;

        EXPECT_NEAR (input, output, 1e-12);
    }
}

TEST (MessageTest, DoubleTest) {
    gpw::net::message<uint16_t> msg;
    random_float_gen<double>    gen;

    for (size_t i = 0; i < max_test_count; ++i) {
        auto input = gen();

        msg << input;

        double output;
        msg >> output;

        EXPECT_NEAR (input, output, 1e-12);
    }
}
