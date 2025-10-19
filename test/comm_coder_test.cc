#include <gtest/gtest.h>

#include <limits>
#include <random>  // For QuickCheck
#include <type_traits>

#include "core/comm_coder.h"

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
    gpw::coder::buffer_t      bfr;
    random_int_gen<data_type> gen;

    for (size_t i = 0; i < max_test_count; ++i) {
        auto input = gen();

        bfr << input;

        data_type output;
        bfr >> output;

        EXPECT_EQ (input, output);
    }
}

TEST (CoderTest, UInt8Test) { test_integral<uint8_t>(); }
TEST (CoderTest, Int8Test) { test_integral<int8_t>(); }
TEST (CoderTest, UInt16Test) { test_integral<uint16_t>(); }
TEST (CoderTest, Int16Test) { test_integral<int16_t>(); }
TEST (CoderTest, UInt32Test) { test_integral<uint32_t>(); }
TEST (CoderTest, Int32Test) { test_integral<int32_t>(); }
TEST (CoderTest, UInt64Test) { test_integral<uint64_t>(); }
TEST (CoderTest, Int64Test) { test_integral<int64_t>(); }

template <typename data_type>
void
test_floating_point () {
    gpw::coder::buffer_t        bfr;
    random_float_gen<data_type> gen;

    for (size_t i = 0; i < max_test_count; ++i) {
        auto input = gen();

        bfr << input;

        data_type output;
        bfr >> output;

        EXPECT_NEAR (input, output, 1e-12);
    }
}

TEST (CoderTest, FloatTest) { test_floating_point<float>(); }
TEST (CoderTest, DoubleTest) { test_floating_point<double>(); }

template <typename data_type>
void
test_vector_integral () {
    random_int_gen<data_type> gen;

    std::vector<data_type> input_v;
    for (size_t i = 0; i < max_test_count; ++i) {
        auto input = gen();
        input_v.push_back (input);
    }

    gpw::coder::buffer_t   bfr = gpw::coder::new_buffer (input_v);
    std::vector<data_type> vec;
    vec = gpw::coder::decode<data_type> (bfr);

    for (size_t i = 0; i < max_test_count; ++i) {
        EXPECT_EQ (input_v[i], vec[i]);
    }
}

TEST (CoderTest, UkInt8VectorTest) { test_vector_integral<uint8_t>(); }
TEST (CoderTest, Int8VectorTest) { test_vector_integral<int8_t>(); }
TEST (CoderTest, UInt16VectorTest) { test_vector_integral<uint16_t>(); }
TEST (CoderTest, Int16VectorTest) { test_vector_integral<int16_t>(); }
TEST (CoderTest, UInt32VectorTest) { test_vector_integral<uint32_t>(); }
TEST (CoderTest, Int32VectorTest) { test_vector_integral<int32_t>(); }
TEST (CoderTest, UInt64VectorTest) { test_vector_integral<uint64_t>(); }
TEST (CoderTest, Int64VectorTest) { test_vector_integral<int64_t>(); }

template <typename data_type>
void
test_vector_floating_point () {
    random_float_gen<data_type> gen;

    std::vector<data_type> input_v;
    for (size_t i = 0; i < max_test_count; ++i) {
        auto input = gen();
        input_v.push_back (input);
    }

    gpw::coder::buffer_t   bfr = gpw::coder::new_buffer (input_v);
    std::vector<data_type> vec;
    vec = gpw::coder::decode<data_type> (bfr);

    for (size_t i = 0; i < max_test_count; ++i) {
        EXPECT_NEAR (input_v[i], vec[i], 1e-12);
    }
}

TEST (CoderTest, FloatVectorTest) { test_vector_floating_point<float>(); }
TEST (CoderTest, DoubleVectorTest) { test_vector_floating_point<double>(); }

void
test_struct () {
    struct test_struct_t {
        uint8_t ui8;
        int8_t  i8;

        uint16_t ui16;
        int16_t  i16;

        uint32_t ui32;
        int32_t  i32;

        uint64_t ui64;
        int64_t  i64;

        float  flt;
        double dbl;
    };

    random_int_gen<uint8_t>  gen_ui8;
    random_int_gen<uint16_t> gen_ui16;
    random_int_gen<uint32_t> gen_ui32;
    random_int_gen<uint64_t> gen_ui64;

    random_int_gen<int8_t>  gen_i8;
    random_int_gen<int16_t> gen_i16;
    random_int_gen<int32_t> gen_i32;
    random_int_gen<int64_t> gen_i64;

    random_float_gen<float>  gen_flt;
    random_float_gen<double> gen_dbl;

    gpw::coder::buffer_t bfr;

    std::vector<test_struct_t> input_v;
    for (size_t i = 0; i < max_test_count; ++i) {
        test_struct_t obj{
            .ui8 = gen_ui8(),
            .i8  = gen_i8(),

            .ui16 = gen_ui16(),
            .i16  = gen_i16(),

            .ui32 = gen_ui32(),
            .i32  = gen_i32(),

            .ui64 = gen_ui64(),
            .i64  = gen_i64(),

            .flt = gen_flt(),
            .dbl = gen_dbl()
        };
        input_v.push_back (obj);

        bfr << obj.ui8 << obj.i8 << obj.ui16 << obj.i16 << obj.ui32 << obj.i32 << obj.ui64
            << obj.i64 << obj.flt << obj.dbl;
    }

    for (size_t i = 0; i < max_test_count; ++i) {
        test_struct_t obj;
        bfr >> obj.dbl >> obj.flt >> obj.i64 >> obj.ui64 >> obj.i32 >> obj.ui32 >> obj.i16 >>
            obj.ui16 >> obj.i8 >> obj.ui8;

        EXPECT_EQ (input_v[max_test_count - i - 1].ui8, obj.ui8);
        EXPECT_EQ (input_v[max_test_count - i - 1].i8, obj.i8);

        EXPECT_EQ (input_v[max_test_count - i - 1].ui16, obj.ui16);
        EXPECT_EQ (input_v[max_test_count - i - 1].i16, obj.i16);

        EXPECT_EQ (input_v[max_test_count - i - 1].ui32, obj.ui32);
        EXPECT_EQ (input_v[max_test_count - i - 1].i32, obj.i32);

        EXPECT_EQ (input_v[max_test_count - i - 1].ui64, obj.ui64);
        EXPECT_EQ (input_v[max_test_count - i - 1].i64, obj.i64);

        EXPECT_NEAR (input_v[max_test_count - i - 1].flt, obj.flt, 1e-12);
        EXPECT_NEAR (input_v[max_test_count - i - 1].dbl, obj.dbl, 1e-12);
    }
}

TEST (CoderTest, StructTest) { test_struct(); }
