#include <stdio.h>
#include <string.h>
#include <mach/mach_time.h>

#include "fmt.h"

#define RED "\x1b[1;31m"
#define GREEN "\x1b[1;32m"
#define RESET "\x1b[0m"

#define BENCH_ITERATIONS 10000

mach_timebase_info_data_t info;

static inline uint64_t get_time_ns(void) {
  uint64_t ts = mach_absolute_time();
  ts *= info.numer;
  ts /= info.denom;
  return ts;
}

static void fmt_test_case(const char *expected, const char *format, ...) __attribute__((optnone)) {
  const size_t size = 4096;
  char buffer[size];

  va_list args;
  va_start(args, format);
  uint64_t start = get_time_ns();
  fmt_format(format, buffer, size, FMT_MAX_ARGS, args);
  uint64_t end = get_time_ns();
  va_end(args);
  uint64_t ns = end - start;

  if (strcmp(buffer, expected) != 0) {
    printf(RED"[FAIL]"RESET" \"%s\" in %llu ns\n", format, ns);
    printf("  expected: \"%s\"\n", expected);
    printf("  actual:   \"%s\"\n", buffer);
    return;
  }

  uint64_t ns_avg;
  for (int i = 0; i < BENCH_ITERATIONS; i++) {
    va_start(args, format);
    start = get_time_ns();
    fmt_format(format, buffer, size, FMT_MAX_ARGS, args);
    end = get_time_ns();
    va_end(args);
    ns += end - start;
  }

  ns_avg = ns / BENCH_ITERATIONS;
  printf(GREEN"[PASS]"RESET" \"%s\" in %llu ns\n", expected, ns_avg);
}

void test_cases_alternate(void) {
  printf("\n=== RUST-LIKE {:} SPECIFIER TESTS ===\n");

  // Basic types
  fmt_test_case("42", "{:d}", 42);
  fmt_test_case("42", "{:u}", 42U);
  fmt_test_case("101010", "{:b}", 42);
  fmt_test_case("52", "{:o}", 42);
  fmt_test_case("2a", "{:x}", 42);
  fmt_test_case("2A", "{:X}", 42);
  fmt_test_case("3.140000", "{:f}", 3.14);
  fmt_test_case("3.140000", "{:F}", 3.14);
  fmt_test_case("3.140000e+00", "{:e}", 3.14);
  fmt_test_case("3.140000E+00", "{:E}", 3.14);
  fmt_test_case("3.14", "{:g}", 3.14);
  fmt_test_case("3.14", "{:G}", 3.14);
  fmt_test_case("0x1p+0", "{:a}", 1.0);
  fmt_test_case("0X1P+0", "{:A}", 1.0);
  fmt_test_case("hello", "{:s}", "hello");
  fmt_test_case("A", "{:c}", 'A');

  // Size modifiers
  fmt_test_case("18446744073709551615", "{:llu}", UINT64_MAX);
  fmt_test_case("-9223372036854775808", "{:lld}", INT64_MIN);
  fmt_test_case("4096", "{:zu}", (size_t)4096);
  fmt_test_case("255", "{:zu}", (size_t)255);

  // Index selection
  fmt_test_case("second first", "{1:s} {0:s}", "first", "second");
  fmt_test_case("3 2 1", "{2:d} {1:d} {0:d}", 1, 2, 3);
  fmt_test_case("hello 42 world", "{0:s} {2:d} {1:s}", "hello", "world", 42);

  // Alignment and fill
  fmt_test_case("42   ", "{:<5d}", 42);
  fmt_test_case("   42", "{:>5d}", 42);
  fmt_test_case("  42 ", "{:^5d}", 42);
  fmt_test_case("**42*", "{:*^5d}", 42);
  fmt_test_case("--42-", "{:$-^5d}", 42);
  fmt_test_case("hello     ", "{:<10s}", "hello");
  fmt_test_case("     hello", "{:>10s}", "hello");
  fmt_test_case("   hello  ", "{:^10s}", "hello");
  fmt_test_case("###hello##", "{:$#^10s}", "hello");

  // Flags
  fmt_test_case("0x2a", "{:#x}", 42);
  fmt_test_case("0X2A", "{:#X}", 42);
  fmt_test_case("0b101010", "{:#b}", 42);
  fmt_test_case("0o52", "{:#o}", 42);
  fmt_test_case("2A", "{:!x}", 42);
  fmt_test_case("00042", "{:05d}", 42);
  fmt_test_case("-0042", "{:05d}", -42);
  fmt_test_case("+42", "{:+d}", 42);
  fmt_test_case("-42", "{:+d}", -42);
  fmt_test_case(" 42", "{: d}", 42);
  fmt_test_case("-42", "{: d}", -42);
  fmt_test_case("42   ", "{:-5d}", 42);

  // Precision
  fmt_test_case("3.14", "{:.2f}", 3.14159);
  fmt_test_case("3.142", "{:.3f}", 3.14159);
  fmt_test_case("3", "{:.0f}", 3.14159);
  fmt_test_case("00042", "{:.5d}", 42);
  fmt_test_case("hello", "{:.10s}", "hello");
  fmt_test_case("hel", "{:.3s}", "hello");
  fmt_test_case("3.14e+00", "{:.2e}", 3.14159);
  fmt_test_case("3.1", "{:.2g}", 3.14);

  // Width
  fmt_test_case("    42", "{:6d}", 42);
  fmt_test_case("  3.14", "{:6.2f}", 3.14);
  fmt_test_case("hello ", "{:6s}", "hello");

  // Dynamic width and precision
  fmt_test_case("    42", "{:*d}", 42, 6);
  fmt_test_case("3.14", "{:.*f}", 3.14159, 2);
  fmt_test_case("  3.14", "{:*.*f}", 3.14159, 6, 2);
  fmt_test_case("    42", "{1:*0d}", 6, 42);
  fmt_test_case("3.14", "{2:.*0f}", 2, 0, 3.14159);

  // Complex combinations
  fmt_test_case("0x002a", "{:#06x}", 42);
  fmt_test_case("+00042", "{:+06d}", 42);
  fmt_test_case("  +3.14", "{:+7.2f}", 3.14);
  fmt_test_case("****42***", "{:*^9d}", 42);
  fmt_test_case("===2a===", "{:$=^8x}", 42);
  fmt_test_case("==0x2a==", "{:$=^#8x}", 42);

  // Special float values
  fmt_test_case("inf", "{:f}", 1.0/0.0);
  fmt_test_case("INF", "{:F}", 1.0/0.0);
  fmt_test_case("-inf", "{:f}", -1.0/0.0);
  fmt_test_case("nan", "{:f}", 0.0/0.0);
  fmt_test_case("NAN", "{:F}", 0.0/0.0);

  // Edge cases
  fmt_test_case("0", "{:d}", 0);
  fmt_test_case("0", "{:x}", 0);
  fmt_test_case("0.000000", "{:f}", 0.0);
  fmt_test_case("(null)", "{:s}", (char*)NULL);
  fmt_test_case("\\0", "{:c}", '\0');

  printf("\n=== PRINTF %% SPECIFIER TESTS ===\n");

  // Basic conversions
  fmt_test_case("42", "%d", 42);
  fmt_test_case("42", "%i", 42);
  fmt_test_case("42", "%u", 42U);
  fmt_test_case("52", "%o", 42);
  fmt_test_case("2a", "%x", 42);
  fmt_test_case("2A", "%X", 42);
  fmt_test_case("3.140000", "%f", 3.14);
  fmt_test_case("3.140000", "%F", 3.14);
  fmt_test_case("3.140000e+00", "%e", 3.14);
  fmt_test_case("3.140000E+00", "%E", 3.14);
  fmt_test_case("3.14", "%g", 3.14);
  fmt_test_case("3.14", "%G", 3.14);
  fmt_test_case("0x1p+0", "%a", 1.0);
  fmt_test_case("0X1P+0", "%A", 1.0);
  fmt_test_case("hello", "%s", "hello");
  fmt_test_case("A", "%c", 'A');
  fmt_test_case("%%", "%%%%");

  // Length modifiers
  fmt_test_case("255", "%hhu", (unsigned char)255);
  fmt_test_case("65535", "%hu", (unsigned short)65535);
  fmt_test_case("4294967295", "%lu", (unsigned long)4294967295UL);
  fmt_test_case("18446744073709551615", "%llu", UINT64_MAX);
  fmt_test_case("4096", "%zu", (size_t)4096);
  fmt_test_case("9223372036854775807", "%jd", (intmax_t)INT64_MAX);
  fmt_test_case("1024", "%td", (ptrdiff_t)1024);

  // Width and precision
  fmt_test_case("   42", "%5d", 42);
  fmt_test_case("42   ", "%-5d", 42);
  fmt_test_case("00042", "%05d", 42);
  fmt_test_case("+42", "%+d", 42);
  fmt_test_case(" 42", "% d", 42);
  fmt_test_case("3.14", "%.2f", 3.14159);
  fmt_test_case("00042", "%.5d", 42);
  fmt_test_case("hello", "%.10s", "hello");
  fmt_test_case("hel", "%.3s", "hello");

  // Dynamic width and precision
  fmt_test_case("    42", "%*d", 6, 42);
  fmt_test_case("3.14", "%.*f", 2, 3.14159);
  fmt_test_case("  3.14", "%*.*f", 6, 2, 3.14159);

  // Positional parameters
  fmt_test_case("world hello", "%2$s %1$s", "hello", "world");
  fmt_test_case("3 2 1", "%3$d %2$d %1$d", 1, 2, 3);
  fmt_test_case("  hello", "%2$*1$s", 7, "hello");
  fmt_test_case("3.14", "%3$.*1$f", 2, 0, 3.14159);

  // Alternate form
  fmt_test_case("0x2a", "%#x", 42);
  fmt_test_case("0X2A", "%#X", 42);
  fmt_test_case("052", "%#o", 42);
  fmt_test_case("3.000000", "%#f", 3.0);

  // Combined flags
  fmt_test_case("0x002a", "%#06x", 42);
  fmt_test_case("+00042", "%+06d", 42);
  fmt_test_case("  +3.14", "%+7.2f", 3.14);
  fmt_test_case("-00042", "%06d", -42);

  // Scientific notation edge cases
  fmt_test_case("1.000000e+00", "%.6e", 1.0);
  fmt_test_case("1.000000e-06", "%.6e", 0.000001);
  fmt_test_case("1.000000e+06", "%.6e", 1000000.0);

  // General format edge cases
  fmt_test_case("0.001", "%g", 0.001);
  fmt_test_case("1e-05", "%g", 0.00001);
  fmt_test_case("100000", "%g", 100000.0);
  fmt_test_case("1e+06", "%g", 1000000.0);

  // Special values
  fmt_test_case("inf", "%f", 1.0/0.0);
  fmt_test_case("INF", "%F", 1.0/0.0);
  fmt_test_case("-inf", "%f", -1.0/0.0);
  fmt_test_case("nan", "%f", 0.0/0.0);
  fmt_test_case("NAN", "%F", 0.0/0.0);

  // Edge cases and error conditions
  fmt_test_case("0", "%d", 0);
  fmt_test_case("0", "%x", 0);
  fmt_test_case("0.000000", "%f", 0.0);
  fmt_test_case("(null)", "%s", (char*)NULL);
  fmt_test_case("\\0", "%c", '\0');

  // Very large/small numbers
  fmt_test_case("9223372036854775807", "%lld", INT64_MAX);
  fmt_test_case("-9223372036854775808", "%lld", INT64_MIN);
  fmt_test_case("18446744073709551615", "%llu", UINT64_MAX);

  printf("\n=== POSIX COMPLIANCE TESTS ===\n");

  // Zero value with explicit precision of 0 produces no characters (POSIX)
  fmt_test_case("", "%.0d", 0);
  fmt_test_case("", "%.0u", 0U);
  fmt_test_case("", "%.0x", 0);
  fmt_test_case("", "%.0o", 0);
  fmt_test_case("42", "%.0d", 42);      // non-zero still works

  // Precision on integers: minimum digits, 0 flag ignored
  fmt_test_case("00042", "%.5d", 42);
  fmt_test_case("  00042", "%7.5d", 42); // width + precision: spaces pad, not zeros
  fmt_test_case("  00042", "%07.5d", 42); // 0 flag ignored when precision specified

  // Flag interactions: + overrides space
  fmt_test_case("+42", "%+ d", 42);      // + and space: + wins
  fmt_test_case("+42", "% +d", 42);      // order doesn't matter

  // Negative dynamic width: treated as '-' flag + positive width
  fmt_test_case("42   ", "%*d", -5, 42);

  // Negative dynamic precision: treated as if precision omitted
  fmt_test_case("3.140000", "%.*f", -1, 3.14);   // default precision 6

  // %#x with zero value: POSIX says "0" (no prefix for zero)
  fmt_test_case("0x0", "%#x", 0);
  fmt_test_case("0", "%#o", 0);          // octal: 0 already has a leading 0

  // Precision with strings
  fmt_test_case("", "%.0s", "hello");     // precision 0 = no chars
  fmt_test_case("h", "%.1s", "hello");
  fmt_test_case("hello", "%.100s", "hello"); // precision > len is fine

  // Width with strings
  fmt_test_case("     hello", "%10s", "hello");  // right-aligned by default
  fmt_test_case("hello     ", "%-10s", "hello"); // left-aligned with -

  // Combining width and precision for strings
  fmt_test_case("       hel", "%10.3s", "hello");  // 3 chars, right-aligned in 10

  // Precision with floats
  fmt_test_case("3", "%.0f", 3.14159);   // no decimal places
  fmt_test_case("3.", "%#.0f", 3.14159);  // # forces decimal point with 0 precision
  fmt_test_case("0.0", "%.1f", 0.0);

  // %% mixed with positional args
  fmt_test_case("100% of 42", "%d%% of %d", 100, 42);

  // Multiple positional args with gaps
  fmt_test_case("c b a", "%3$s %2$s %1$s", "a", "b", "c");

  // Width with positional
  fmt_test_case("    42", "%1$*2$d", 42, 6);

  printf("\n=== MIXED FORMAT TESTS ===\n");

  // Mixed {:} and % in same format string
  fmt_test_case("42 and 3.14", "{:d} and %.2f", 42, 3.14);
  fmt_test_case("hex 2a and dec 42", "hex {:x} and dec %d", 42, 42);
  fmt_test_case("first 1 second 2.50", "first {:d} second %.2f", 1, 2.5);

  // Complex real-world scenarios
  fmt_test_case("Error 404: File 'test.txt' not found", "Error {:d}: File '{:s}' not found", 404, "test.txt");
  fmt_test_case("Process [1234] completed in 0.125s", "Process [{:d}] completed in {:.3f}s", 1234, 0.125);
  fmt_test_case("Memory: 1024KB (0x400 bytes)", "Memory: {:d}KB ({:#x} bytes)", 1024, 1024);
  fmt_test_case("Temperature: +23.5°C (74.3°F)", "Temperature: {:+.1f}°C ({:.1f}°F)", 23.5, 74.3);

  printf("\n=== TEST COMPLETE ===\n");
}


int main(int argc, char **argv) {
  mach_timebase_info(&info);

  test_cases_alternate();
  return 0;

  // basic
  fmt_test_case("Hello, world!", "Hello, world!");
  fmt_test_case("Hello, world!", "{:s}", "Hello, world!");
  fmt_test_case("42", "{:d}", 42);
  fmt_test_case("2a", "{:x}", 42);
  fmt_test_case("3.14", "{:.2f}", 3.14);
  fmt_test_case("3.14, 42", "{:.2f}, {:d}", 3.14, 42);

  // index
  fmt_test_case("42, 3.14", "{1:d}, {0:.2f}", 3.14, 42);
  fmt_test_case("3.14, string, 42", "{0:.2f}, {2:s}, {1:d}", 3.14, 42, "string");

  // flags
  fmt_test_case("0x2a", "{:#x}", 42);
  fmt_test_case("2A", "{:!x}", 42);
  fmt_test_case("007", "{:03d}", 7);
  fmt_test_case("-007", "{:04d}", -7);
  fmt_test_case("+007", "{:+04d}", 7);
  fmt_test_case("1   ", "{:4d}", 1);
  fmt_test_case("   1", "{:-4d}", 1);
  fmt_test_case(" 42", "{: d}", 42);
  fmt_test_case("-42", "{: d}", -42);
  fmt_test_case("3", "{:#.1f}", 3.f);
  fmt_test_case("3.1", "{:#.1f}", 3.1);

  // alignment/fill
  fmt_test_case("42  ", "{:4d}", 42);
  fmt_test_case(" 42 ", "{:^4d}", 42);
  fmt_test_case("  42", "{:>4d}", 42);
  fmt_test_case("===== hello =====", "{:$=^17s}", " hello ");
  fmt_test_case("............101", "{:$.>*b}", 5, 15);
  fmt_test_case("101............", "{1:$.<*0b}", 15, 5);
  fmt_test_case("               ", "{:$ >*}", 15);
  fmt_test_case("          ", "{:10}"); // zero-arg fill

  // printf
  fmt_test_case("42", "%d", 42);
  fmt_test_case("2a", "%x", 42);
  fmt_test_case("3.14", "%.2f", 3.14);
  fmt_test_case("FFFFFFFFFFFFFFFF", "%llX", UINT64_MAX);
  fmt_test_case("1, hi, f", "%d, %s, %x", 1, "hi", 15);
  fmt_test_case("->  <-", "-> %J <-", 1); // unknown format specifier

  // POSIX compliance tests
  fmt_test_case("42", "%i", 42);               // %i format
  fmt_test_case("%", "%%");                    // %% literal
  fmt_test_case("42 24", "%2$d %1$d", 24, 42); // positional parameters
  fmt_test_case("  42", "%4d", 42);            // width
  fmt_test_case("42  ", "%-4d", 42);           // left align
  fmt_test_case("0042", "%04d", 42);           // zero padding
  fmt_test_case("+42", "%+d", 42);             // sign flag
  fmt_test_case(" 42", "% d", 42);             // space flag
  fmt_test_case("3.14e+00", "%.2e", 3.14);     // scientific notation
  fmt_test_case("3.14E+00", "%.2E", 3.14);     // uppercase scientific
  fmt_test_case("3.1", "%.2g", 3.14);          // general format
  fmt_test_case("3.1", "%.2G", 3.14);          // uppercase general
  fmt_test_case("   42", "%*d", 5, 42);        // dynamic width
  fmt_test_case("3.14", "%.*f", 2, 3.14159);   // dynamic precision
  fmt_test_case("test", "%2$*1$s", 4, "test"); // positional width

  return 0;
}
