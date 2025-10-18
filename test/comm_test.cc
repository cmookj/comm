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

template <typename data_type>
void
test_integral () {
    gpw::net::message<uint16_t> msg;
    random_int_gen<data_type>   gen;

    for (size_t i = 0; i < max_test_count; ++i) {
        auto input = gen();

        msg << input;

        data_type output;
        msg >> output;

        EXPECT_EQ (input, output);
    }
}

TEST (MessageTest, UInt8Test) { test_integral<uint8_t>(); }
TEST (MessageTest, Int8Test) { test_integral<int8_t>(); }
TEST (MessageTest, UInt16Test) { test_integral<uint16_t>(); }
TEST (MessageTest, Int16Test) { test_integral<int16_t>(); }
TEST (MessageTest, UInt32Test) { test_integral<uint32_t>(); }
TEST (MessageTest, Int32Test) { test_integral<int32_t>(); }
TEST (MessageTest, UInt64Test) { test_integral<uint64_t>(); }
TEST (MessageTest, Int64Test) { test_integral<int64_t>(); }

template <typename data_type>
void
test_floating_point () {
    gpw::net::message<uint16_t> msg;
    random_float_gen<data_type> gen;

    for (size_t i = 0; i < max_test_count; ++i) {
        auto input = gen();

        msg << input;

        data_type output;
        msg >> output;

        EXPECT_NEAR (input, output, 1e-12);
    }
}

TEST (MessageTest, FloatTest) { test_floating_point<float>(); }
TEST (MessageTest, DoubleTest) { test_floating_point<double>(); }

template <typename data_type>
void
test_vector_integral () {
    gpw::net::message<uint16_t> msg;
    random_int_gen<data_type>   gen;

    std::vector<data_type> input_v;
    for (size_t i = 0; i < max_test_count; ++i) {
        auto input = gen();
        msg << input;
        input_v.push_back (input);
    }

    std::vector<data_type> vec;
    vec = msg.decode<data_type>();

    for (size_t i = 0; i < max_test_count; ++i) {
        EXPECT_EQ (input_v[i], vec[i]);
    }
}

TEST (MessageTest, UkInt8VectorTest) { test_vector_integral<uint8_t>(); }
TEST (MessageTest, Int8VectorTest) { test_vector_integral<int8_t>(); }
TEST (MessageTest, UInt16VectorTest) { test_vector_integral<uint16_t>(); }
TEST (MessageTest, Int16VectorTest) { test_vector_integral<int16_t>(); }
TEST (MessageTest, UInt32VectorTest) { test_vector_integral<uint32_t>(); }
TEST (MessageTest, Int32VectorTest) { test_vector_integral<int32_t>(); }
TEST (MessageTest, UInt64VectorTest) { test_vector_integral<uint64_t>(); }
TEST (MessageTest, Int64VectorTest) { test_vector_integral<int64_t>(); }

template <typename data_type>
void
test_vector_floating_point () {
    gpw::net::message<uint16_t> msg;
    random_float_gen<data_type> gen;

    std::vector<data_type> input_v;
    for (size_t i = 0; i < max_test_count; ++i) {
        auto input = gen();
        msg << input;
        input_v.push_back (input);
    }

    std::vector<data_type> vec;
    vec = msg.decode<data_type>();

    for (size_t i = 0; i < max_test_count; ++i) {
        EXPECT_NEAR (input_v[i], vec[i], 1e-12);
    }
}

TEST (MessageTest, FloatVectorTest) { test_vector_floating_point<float>(); }
TEST (MessageTest, DoubleVectorTest) { test_vector_floating_point<double>(); }
