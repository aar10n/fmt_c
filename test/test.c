#include <stdio.h>
#include <string.h>
#include <mach/mach_time.h>

#include <fmt.h>
#include <fmtlib.h>

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


// custom formatter

struct my_struct {
  int a;
  int b;
};

const fmt_argtype_t my_argtype = FMT_ARGTYPE_VOIDPTR; // take by reference

static size_t my_formatter(fmt_buffer_t *buffer, const fmt_spec_t *spec) {
  struct my_struct *s = spec->value.voidptr_value;
  return fmt_write(buffer, "{{{:d}, {:d}}}", s->a, s->b);
}

int main(int argc, char **argv) {
  mach_timebase_info(&info);

  fmtlib_register_type("test", my_formatter, my_argtype);

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
  fmt_test_case(" 42", "{: d}", 42);
  fmt_test_case("-42", "{: d}", -42);

  // alignment/fill
  fmt_test_case("  42", "{:4d}", 42);
  fmt_test_case(" 42 ", "{:^4d}", 42);
  fmt_test_case("42  ", "{:>4d}", 42);
  fmt_test_case("===== hello =====", "{:$=^17s}", " hello ");
  fmt_test_case("101............", "{:$.>*b}", 5, 15);
  fmt_test_case("............101", "{1:$.<*0b}", 15, 5);

  // custom formatter
  struct my_struct s = { 42, 3 };
  fmt_test_case("{42, 3}", "{:test}", &s);

  return 0;
}
