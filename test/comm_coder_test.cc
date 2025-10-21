#include <gtest/gtest.h>

#include <limits>
#include <random>  // For QuickCheck
#include <type_traits>

#include "core/comm_coder.h"

constexpr size_t max_test_count = 65536;

template <typename T> struct random_int_gen {
    // By default, random integral numbers are generated between the minimum
    // and maximum possible values.
    random_int_gen (
        const T lower = std::numeric_limits<T>::min(),
        const T upper = std::numeric_limits<T>::max()
    ) {
        // Create a random device to seed the generator
        std::random_device rd;

        // Use the Mersenne Twister engine
        gen = std::mt19937 (rd());

        // Define distributions
        dist = std::uniform_int_distribution<T> (lower, upper);
    }

    T
    operator() () {
        return dist (gen);
    }

    std::mt19937                     gen;
    std::uniform_int_distribution<T> dist;
};

template <typename T> struct random_float_gen {
    // By default, random floating point numbers are generated between 0.0 and 1.0
    random_float_gen (const T lower = T (0.), const T upper = T (1.)) {
        // Create a random device to seed the generator
        std::random_device rd;

        // Use the Mersenne Twister engine
        gen = std::mt19937 (rd());

        // Define distributions
        if constexpr (!std::is_floating_point_v<T>)
            throw std::runtime_error ("The template parameter should be floating_point");

        dist = std::uniform_real_distribution<T> (lower, upper);
    }

    T
    operator() () {
        return dist (gen);
    }

    std::mt19937                      gen;
    std::uniform_real_distribution<T> dist;
};

struct random_string_gen {
    random_string_gen (const uint8_t max_len = 40) {
        // The length of the random string is randomly picked between 0 and 40.
        dice_len = random_int_gen<uint8_t>{0, max_len};

        // Random characters are generated between 48(0x30) and 122(0x7a) which
        // correspond to '0' and 'z', respectively.
        dice = random_int_gen<uint8_t>{48, 122};
    }

    std::string
    operator() () {
        uint8_t     len = dice_len();
        std::string random_str (len, ' ');
        std::generate (random_str.begin(), random_str.end(), dice);
        return random_str;
    }

    random_int_gen<uint8_t> dice;
    // Random number generator for the length of the arbitrary string generated.
    random_int_gen<uint8_t> dice_len;
};

//// Test functions for a single element.
template <typename data_type>
void
test_integral () {
    gpw::coder::coder_t       cdr;
    random_int_gen<data_type> gen;

    for (size_t i = 0; i < max_test_count; ++i) {
        auto input = gen();

        cdr << input;

        data_type output;
        cdr >> output;

        EXPECT_EQ (input, output);
    }
}

template <typename data_type>
void
test_floating_point () {
    gpw::coder::coder_t         cdr;
    random_float_gen<data_type> gen;

    for (size_t i = 0; i < max_test_count; ++i) {
        auto input = gen();

        cdr << input;

        data_type output;
        cdr >> output;

        EXPECT_NEAR (input, output, 1e-12);
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
TEST (CoderTest, FloatTest) { test_floating_point<float>(); }
TEST (CoderTest, DoubleTest) { test_floating_point<double>(); }

// Test for a single string.
TEST (CoderTest, StringTest) {
    gpw::coder::coder_t cdr;
    random_string_gen   gen;

    for (size_t i = 0; i < max_test_count; ++i) {
        auto input = gen();

        cdr << input;

        std::string output;
        cdr >> output;

        EXPECT_EQ (input, output);
    }
}

//// Test functions for a vector.
template <typename data_type>
void
test_vector_integral () {
    random_int_gen<data_type> gen;

    std::vector<data_type> input_v;
    for (size_t i = 0; i < max_test_count; ++i) {
        auto input = gen();
        input_v.push_back (input);
    }

    gpw::coder::coder_t cdr;
    cdr.to_buffer (input_v);

    std::vector<data_type> vec;
    vec = cdr.to_vector<data_type>();

    for (size_t i = 0; i < max_test_count; ++i) {
        EXPECT_EQ (input_v[i], vec[i]);
    }
}

template <typename data_type>
void
test_vector_floating_point () {
    random_float_gen<data_type> gen;

    std::vector<data_type> input_v;
    for (size_t i = 0; i < max_test_count; ++i) {
        auto input = gen();
        input_v.push_back (input);
    }

    gpw::coder::coder_t cdr;
    cdr.to_buffer (input_v);

    std::vector<data_type> vec;
    vec = cdr.to_vector<data_type>();

    for (size_t i = 0; i < max_test_count; ++i) {
        EXPECT_NEAR (input_v[i], vec[i], 1e-12);
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
TEST (CoderTest, FloatVectorTest) { test_vector_floating_point<float>(); }
TEST (CoderTest, DoubleVectorTest) { test_vector_floating_point<double>(); }
TEST (CoderTest, StringVectorTest) {
    random_string_gen gen;

    std::vector<std::string> input_v;
    for (size_t i = 0; i < max_test_count; ++i) {
        auto input = gen();
        input_v.push_back (input);
    }

    gpw::coder::coder_t cdr;
    cdr.to_buffer (input_v);

    std::vector<std::string> vec;
    vec = cdr.to_vector<std::string>();

    for (size_t i = 0; i < max_test_count; ++i) {
        EXPECT_EQ (input_v[i], vec[i]);
    }
}

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

    std::string str1;
    std::string str2;
};

bool
operator== (const test_struct_t& ll, const test_struct_t& rr) {
    return ll.ui8 == rr.ui8 && ll.i8 == rr.i8 && ll.ui16 == rr.ui16 && ll.i16 == rr.i16 &&
           ll.ui32 == rr.ui32 && ll.i32 == rr.i32 && ll.ui64 == rr.ui64 && ll.i64 == rr.i64 &&
           ll.flt == rr.flt && ll.dbl == rr.dbl && ll.str1 == rr.str1 && ll.str2 == rr.str2;
}

TEST (CoderTest, StructTest) {
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

    random_string_gen gen_str;

    gpw::coder::coder_t cdr;

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
            .dbl = gen_dbl(),

            .str1 = gen_str(),
            .str2 = gen_str(),
        };
        input_v.push_back (obj);

        cdr << obj.ui8 << obj.i8 << obj.ui16 << obj.i16 << obj.str1 << obj.ui32 << obj.i32
            << obj.ui64 << obj.i64 << obj.str2 << obj.flt << obj.dbl;
    }

    for (size_t i = 0; i < max_test_count; ++i) {
        test_struct_t obj;
        cdr >> obj.dbl >> obj.flt >> obj.str2 >> obj.i64 >> obj.ui64 >> obj.i32 >> obj.ui32 >>
            obj.str1 >> obj.i16 >> obj.ui16 >> obj.i8 >> obj.ui8;

        EXPECT_TRUE (input_v[max_test_count - i - 1] == obj);
    }
}

#if false
TEST (CoderTest, StructVectorTest) {
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

    random_string_gen gen_str;

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
            .dbl = gen_dbl(),

            .str1 = gen_str(),
            .str2 = gen_str(),
        };
        input_v.push_back (obj);
    }

    gpw::coder::buffer_t       bfr = gpw::coder::to_buffer (input_v);
    std::vector<test_struct_t> vec;
    vec = gpw::coder::to_vector<test_struct_t> (bfr);

    for (size_t i = 0; i < max_test_count; ++i) {
        EXPECT_TRUE (input_v[i] == vec[i]);
    }
}
#endif
