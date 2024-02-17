// Copyright 2014 BitPay Inc.
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <string.h>
#include <univalue.h>

#include <cassert>
#include <cstdio>
#include <iostream>
#include <string>

#ifndef JSON_TEST_SRC
#error JSON_TEST_SRC must point to test source directory
#endif

std::string srcdir(JSON_TEST_SRC);

static std::string rtrim(std::string s)
{
    s.erase(s.find_last_not_of(" \n\r\t")+1);
    return s;
}

static void runtest(std::string filename, const std::string& jdata)
{
        std::string prefix = filename.substr(0, 4);

        bool wantPass = (prefix == "pass") || (prefix == "roun");
        bool wantFail = (prefix == "fail");
        bool wantRoundTrip = (prefix == "roun");
        assert(wantPass || wantFail);

        UniValue val;
        bool testResult = val.read(jdata);
        std::cout << "Running " << filename << std::endl;

        if (wantPass) {
            assert(testResult == true);
        } else {
            assert(testResult == false);
        }

        if (wantRoundTrip) {
            std::string odata = val.write(0, 0);
            assert(odata == rtrim(jdata));
        }
}

static void runtest_file(const char *filename_)
{
        std::string basename(filename_);
        std::string filename = srcdir + "/" + basename;
        FILE *f = fopen(filename.c_str(), "r");
        assert(f != nullptr);

        std::string jdata;

        char buf[4096];
        while (!feof(f)) {
                int bread = fread(buf, 1, sizeof(buf), f);
                assert(!ferror(f));

                std::string s(buf, bread);
                jdata += s;
        }

        assert(!ferror(f));
        fclose(f);

        runtest(basename, jdata);
}

static const char *filenames[] = {
        "fail10.json",
        "fail11.json",
        "fail12.json",
        "fail13.json",
        "fail14.json",
        "fail15.json",
        "fail16.json",
        "fail17.json",
        "fail18.json",               // Open and closing [..] exceed MAX_JSON_DEPTH.
        "fail19.json",
        "fail1.json",
        "fail20.json",
        "fail21.json",
        "fail22.json",
        "fail23.json",
        "fail24.json",
        "fail25.json",
        "fail26.json",
        "fail27.json",
        "fail28.json",
        "fail29.json",
        "fail2.json",
        "fail30.json",
        "fail31.json",
        "fail32.json",
        "fail33.json",
        "fail34.json",
        "fail35.json",
        "fail36.json",
        "fail37.json",
        "fail38.json",               // invalid unicode: only first half of surrogate pair
        "fail39.json",               // invalid unicode: only second half of surrogate pair
        "fail40.json",               // invalid unicode: broken UTF-8
        "fail41.json",               // invalid unicode: unfinished UTF-8
        "fail42.json",               // valid json with garbage following a nul byte
        "fail44.json",               // unterminated string
        "fail45.json",               // nested beyond max depth
        "fail3.json",
        "fail4.json",                // extra comma
        "fail5.json",
        "fail6.json",
        "fail7.json",
        "fail8.json",
        "fail9.json",               // extra comma
        "pass1.json",
        "pass2.json",
        "pass3.json",
        "pass4.json",
        "pass5.json",               // 512 nested [..] do work: See MAX_JSON_DEPTH.
        "round1.json",              // round-trip test
        "round2.json",              // unicode
        "round3.json",              // bare string
        "round4.json",              // bare number
        "round5.json",              // bare true
        "round6.json",              // bare false
        "round7.json",              // bare null
};

// Test \u handling
void unescape_unicode_test()
{
    UniValue val;
    bool testResult;
    // Escaped ASCII (quote)
    testResult = val.read("[\"\\u0022\"]");
    assert(testResult);
    assert(val[0].get_str() == "\"");
    // Escaped Basic Plane character, two-byte UTF-8
    testResult = val.read("[\"\\u0191\"]");
    assert(testResult);
    assert(val[0].get_str() == "\xc6\x91");
    // Escaped Basic Plane character, three-byte UTF-8
    testResult = val.read("[\"\\u2191\"]");
    assert(testResult);
    assert(val[0].get_str() == "\xe2\x86\x91");
    // Escaped Supplementary Plane character U+1d161
    testResult = val.read("[\"\\ud834\\udd61\"]");
    assert(testResult);
    assert(val[0].get_str() == "\xf0\x9d\x85\xa1");
}

void no_nul_test()
{
    char buf[] = "___[1,2,3]___";
    UniValue val;
    assert(val.read({buf + 3, 7}));
}

void raw_equal_end()
{
    std::string tokenVal = "";
    unsigned int consumed = 0;
    char foo = '[';
    enum jtokentype result = getJsonToken(tokenVal, consumed, &foo, &foo);
    assert(result == JTOK_NONE);
    assert(consumed == 0);
    assert(tokenVal == "");
}

void no_reading_beyond_end_test(const char* buf, unsigned int size, enum jtokentype expectedResult)
{
    std::string tokenVal = "";
    unsigned int consumed = 0;

    // With uninitialized data starting at `end`.
    // This will cause `valgrind -tool=memcheck -s ./univalue/test/unitester`
    // to fail if we read past end.
    for(unsigned int i = 1; i < size; ++i) {
        std::vector<char> copy(i);
        ::mempcpy(copy.data(), buf, i);
        enum jtokentype result = getJsonToken(tokenVal, consumed, copy.data(), copy.data() + i);
        assert(result == JTOK_ERR);
        assert(consumed == 0);
        assert(tokenVal == "");
    }

    // With data behind the area from raw ... end. We would get different results
    // if we consider the data past end.
    for(unsigned int i = 1; i < size; ++i) {
        enum jtokentype result = getJsonToken(tokenVal, consumed, buf, buf + i);
        assert(result == JTOK_ERR);
        assert(consumed == 0);
        assert(tokenVal == "");
    }

    // If end points to the end, we are good and it should be read.
    enum jtokentype result = getJsonToken(tokenVal, consumed, buf, buf + sizeof(buf));
    assert(result == expectedResult);
    assert(consumed == size);
    assert(tokenVal == "");
}

void parsing_numbers_beyond_end_test(const std::string& toParse, size_t size,
        enum jtokentype expectedResult, unsigned int expectedConsumed = 0,
        const std::string& expectedTokenVal = "") {
    assert(size <= toParse.size());  // The size we want to allow parsing.
    std::vector<char> copy(size);
    ::mempcpy(copy.data(), toParse.data(), size);

    std::string tokenVal = "";
    unsigned int consumed = 0;
    enum jtokentype result;

    // This will fail valgrind.
    result = getJsonToken(tokenVal, consumed, copy.data(), copy.data() + size);
    assert(result == expectedResult);
    assert(consumed == expectedConsumed);
    assert(tokenVal == expectedTokenVal);

    // Now make a test-case that will fail also in regular mode because the code
    // might be reading whats at end and afer.
    tokenVal = "";
    consumed = 0;
    result = getJsonToken(tokenVal, consumed, toParse.data(), toParse.data() + size);
    assert(result == expectedResult);
    assert(consumed == expectedConsumed);
    assert(tokenVal == expectedTokenVal);
}

void exhaustive_short_string_test() {
  // 3 runs 16777216 iterations, which is fast - 3.7s under valgrind.
  // 4 runs 4294967296 iterations: 21 sec w/o valgrind; 13 minutes with.
  // 5 runs 1099511627776: about 1.5 hours w/o valgrind.
  const int bytes_enumerated = 3;
  assert(bytes_enumerated <= 7);  // otherwise we overflow uint64_t.
  for(int num_chars = 1; num_chars <= bytes_enumerated; ++num_chars) {
    std::vector<char> data(num_chars);
    unsigned long long max_data_idx = 1ULL << (num_chars * 8);
    std::cout << "Running " << max_data_idx << " iterations for inputs of " << num_chars << " bytes." << std::endl;
    for(size_t data_idx = 0; data_idx < max_data_idx; ++data_idx) {
      // Fill in data by converting data_idx into a base-256 number.
      // We mirror the bytes in data to have the char_idx be simpler.
      int char_idx = 0;
      size_t num = data_idx;
      while (num > 0) {
        assert(char_idx >= 0);
        assert(char_idx < num_chars);
        data[char_idx] = num % 256;
        num /= 256;
        ++char_idx;
      }
      // Call getJsonToken on the input.
      std::string tokenVal = "";
      unsigned int consumed = 0;
      getJsonToken(tokenVal, consumed, data.data(), data.data() + num_chars);
    }
  }
}

int main (int argc, char *argv[])
{
    for (const auto& f: filenames) {
        runtest_file(f);
    }

    unescape_unicode_test();
    no_nul_test();
    raw_equal_end();
    no_reading_beyond_end_test("true", 4, JTOK_KW_TRUE);
    no_reading_beyond_end_test("null", 4, JTOK_KW_NULL);
    no_reading_beyond_end_test("false", 5, JTOK_KW_FALSE);

    // A single '-' not OK.
    parsing_numbers_beyond_end_test("-0", 1, JTOK_ERR);
    // -0 is OK.
    parsing_numbers_beyond_end_test("-0", 2, JTOK_NUMBER, 2, "-0");

    // Leading 0s not OK.
    parsing_numbers_beyond_end_test("01", 2, JTOK_ERR);
    // However, OK if we are only read the 0.
    parsing_numbers_beyond_end_test("01", 1, JTOK_NUMBER, 1, "0");

    // Leading 0s not OK, with sign.
    parsing_numbers_beyond_end_test("-01", 3, JTOK_ERR);
    // However, OK if we are only reading 2 characters.
    parsing_numbers_beyond_end_test("-01", 2, JTOK_NUMBER, 2, "-0");

    parsing_numbers_beyond_end_test("-x", 2, JTOK_ERR);
    parsing_numbers_beyond_end_test("-5", 2, JTOK_NUMBER, 2, "-5");

    parsing_numbers_beyond_end_test("123456", 6, JTOK_NUMBER, 6, "123456");
    // Do not read past end.
    parsing_numbers_beyond_end_test("123456", 3, JTOK_NUMBER, 3, "123");

    // Ending with . is not OK.
    parsing_numbers_beyond_end_test("0.", 2, JTOK_ERR);

    // 0.0..0 is OK.
    parsing_numbers_beyond_end_test("0.0", 3, JTOK_NUMBER, 3, "0.0");
    parsing_numbers_beyond_end_test("0.000", 5, JTOK_NUMBER, 5, "0.000");

    // Exponents.
    parsing_numbers_beyond_end_test("0.0e1", 5, JTOK_NUMBER, 5, "0.0e1");
    // ending with e is not OK. Note how we only allow to read 4 characters.
    parsing_numbers_beyond_end_test("0.0e1", 4, JTOK_ERR);

    parsing_numbers_beyond_end_test("0.0e+1", 6, JTOK_NUMBER, 6, "0.0e+1");
    parsing_numbers_beyond_end_test("0.0e-1", 6, JTOK_NUMBER, 6, "0.0e-1");

    // End at end. Note how we allow only 5 characters.
    parsing_numbers_beyond_end_test("0.0231", 5, JTOK_NUMBER, 5, "0.023");
    parsing_numbers_beyond_end_test("0.0231", 5, JTOK_NUMBER, 5, "0.023");

    parsing_numbers_beyond_end_test("0.0e+123", 8, JTOK_NUMBER, 8, "0.0e+123");
    parsing_numbers_beyond_end_test("0.0e-123", 8, JTOK_NUMBER, 8, "0.0e-123");

    // End and end.
    parsing_numbers_beyond_end_test("0.0e+123", 7, JTOK_NUMBER, 7, "0.0e+12");
    parsing_numbers_beyond_end_test("0.0e-123", 7, JTOK_NUMBER, 7, "0.0e-12");

    // For valgrind to find accesses past input string.
    exhaustive_short_string_test();

    return 0;
}

