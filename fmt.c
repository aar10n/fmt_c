//
// Created by Aaron Gill-Braun on 2023-02-25.
//

#include "fmt.h"
#include "fmtlib.h"

#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCDFAInspection"

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define is_digit(ch) ((ch) >= '0' && (ch) <= '9')
#define is_alpha(ch) (((ch) >= 'a' && (ch) <= 'z') || ((ch) >= 'A' && (ch) <= 'Z'))
#define is_align(ch) ((ch) == '<' || (ch) == '^' || (ch) == '>')

typedef struct parsed_fmt_spec {
  int index;
  int flags;
  int width_or_index;
  bool width_is_index;
  int precision_or_index;
  bool precision_is_index;
  fmt_align_t align;
  char fill_char;
  const char *type;
  size_t type_len;
  bool valid;
} parsed_fmt_spec_t;

// _Static_assert(sizeof(double) == sizeof(uint64_t), "double and uint64_t must be the same size");
static inline int read_int(const char **ptr) {
  const char *start = *ptr;
  while (is_digit(**ptr)) {
    (*ptr)++;
  }
  return fmtlib_atoi(start, *ptr - start);
}

size_t parse_fmt_spec(const char *format, int max_args, int *arg_index, int *arg_count, parsed_fmt_spec_t *spec) {
#define CHECK_MAX_ARGS(idx) ({ \
    if ((idx) >= max_args) {  \
      goto early_exit;            \
    }                             \
  })
#define CHECK_EOF(ptr) ({ \
    if (*(ptr) == '}')    \
      goto parse_type;    \
    if (*(ptr) == 0)      \
      goto early_exit;    \
  })

  if (*format != '{') {
    return 0;
  }

  // {[index]:[[$fill]align][flags][width][.precision][type]}
  // ^ format
  const char *start;
  const char *ptr = format + 1;

  int index = 0;
  int flags = 0;
  int width_or_index = 0;
  bool width_is_index = false;
  int precision_or_index = 0;
  bool precision_is_index = false;
  fmt_align_t align = FMT_ALIGN_LEFT;
  char fill_char = ' ';
  int new_arg_index = *arg_index;

  // ====== index ======
  CHECK_EOF(ptr);
  if (is_digit(*ptr)) {
    index = read_int(&ptr);
    CHECK_MAX_ARGS(index);
  } else {
    CHECK_MAX_ARGS(new_arg_index);
    index = new_arg_index;
    new_arg_index++;
  }

  if (*ptr == '}') {
    goto parse_type;
  } else if (*ptr == ':') {
    ptr++;
  } else {
    goto early_exit;
  }

  // quick check for fast path
  if (is_alpha(*ptr)) {
    goto parse_type;
  } else if (*ptr == '0') {
    goto parse_flags;
  } else if (is_digit(*ptr)) {
    goto parse_width;
  } else if (*ptr == '.') {
    goto parse_precision;
  }

    // ====== align ======
  CHECK_EOF(ptr);
  if (*ptr == '$') {
    ptr++;
    if (*ptr == 0)
      goto early_exit;

    fill_char = *ptr++;
    if (!is_align(*ptr))
      goto early_exit;
  }

  switch (*ptr) {
    case '<': align = FMT_ALIGN_LEFT; ptr++;
      break;
    case '^': align = FMT_ALIGN_CENTER; ptr++;
      break;
    case '>': align = FMT_ALIGN_RIGHT; ptr++;
      break;
  }

  // ====== flags ======
  CHECK_EOF(ptr);
  flags = 0;
parse_flags:
  switch (*ptr) {
    case '#': flags |= FMT_FLAG_ALT; ptr++;
      goto parse_flags;
    case '!': flags |= FMT_FLAG_UPPER; ptr++;
      goto parse_flags;
    case '0': flags |= FMT_FLAG_ZERO; fill_char = '0'; ptr++;
      goto parse_flags;
    case '+': flags |= FMT_FLAG_SIGN; ptr++;
      goto parse_flags;
    case ' ': flags |= FMT_FLAG_SPACE; ptr++;
      goto parse_flags;
  }

  // ====== width ======
  CHECK_EOF(ptr);
parse_width:
  if (is_digit(*ptr)) {
    width_or_index = read_int(&ptr);
    width_is_index = false;
  } else if (*ptr == '*') {
    ptr++;
    if (*ptr == 0) {
      goto early_exit;
    } else if (is_digit(*ptr)) {
      width_or_index = read_int(&ptr);
      width_is_index = true;
      CHECK_MAX_ARGS(width_or_index);
    } else {
      CHECK_MAX_ARGS(new_arg_index);
      width_or_index = new_arg_index;
      width_is_index = true;
      new_arg_index++;
    }
  }

  // ====== precision ======
  CHECK_EOF(ptr);
parse_precision:
  if (*ptr == '.') {
    ptr++;
    if (is_digit(*ptr)) {
      precision_or_index = read_int(&ptr);
      precision_is_index = false;
    } else if (*ptr == '*') {
      ptr++;
      if (*ptr == 0) {
        goto early_exit;
      } else if (is_digit(*ptr)) {
        precision_or_index = read_int(&ptr);
        precision_is_index = true;
        CHECK_MAX_ARGS(precision_or_index);
      } else {
        CHECK_MAX_ARGS(new_arg_index);
        precision_or_index = new_arg_index;
        precision_is_index = false;
        new_arg_index++;
      }
    } else {
      goto early_exit;
    }
  }

  // ====== type ======
parse_type:
  start = ptr;
  while (*ptr && *ptr != '}') {
    ptr++;
  }
  if (*ptr == 0)
    goto early_exit;

  // ====== finish ======
  spec->index = index;
  spec->flags = flags;
  spec->width_or_index = width_or_index;
  spec->width_is_index = width_is_index;
  spec->precision_or_index = precision_or_index;
  spec->precision_is_index = precision_is_index;
  spec->align = align;
  spec->fill_char = fill_char;
  spec->type = start;
  spec->type_len = ptr - start;
  spec->valid = true;

  int max_arg_index = index;
  if (width_is_index)
    max_arg_index = max(max_arg_index, width_or_index);
  if (precision_is_index)
    max_arg_index = max(max_arg_index, precision_or_index);

  *arg_count = max(*arg_count, max_arg_index + 1);
  *arg_index = new_arg_index;
  return ptr - format + 1;

  //
  // ERROR
early_exit:
  // something went wrong, write nothing and skip to end of format string
  start = format;
  while (*format && *format != '}') {
    format++;
  }

  spec->valid = false;
  return format - start + (*format == '}' ? 1 : 0);
#undef CHECK_EOF
#undef CHECK_MAX_ARGS
}

//

size_t fmt_format(const char *format, char *buffer, size_t size, int max_args, va_list args) {
#define LOAD_ARG(index) ({ \
    void *v;                \
    switch (arg_sizes[index]) { \
      case 0: v = NULL; break; \
      case 4: v = (void *)((uint64_t)va_arg(args_copy, int)); break; \
      case 8: v = (void *)va_arg(args_copy, uint64_t); break; \
      default: v = NULL; break; \
    }                         \
    v;                         \
  })

  va_list args_copy;
  va_copy(args_copy, args);

  size_t n = 0;
  fmt_buffer_t buf = fmt_buffer(buffer, size);

  // the formatter has two different modes of operation depending on the format string.
  // it always starts in single-pass mode, in which it writes to the buffer as it scans
  // the format string. the only time it switches to two-pass mode is when it encounters
  // a specifier that references an argument index greater than the number of arguments
  // read so far. in this case, we have to parse the rest of the format string to determine
  // the size of each argument, load them, and then write it all to the buffer.
  bool single_pass = true;

  // we keep three counters to track arguments. arg_index is used to track implicitly indexed
  // arguments, arg_count to track the largest argument referenced by a specifier and finally
  // loaded_arg_count which tracks the number of arguments read with va_arg. the last counter
  // is only used in two-pass mode because in single-pass mode arg_index == loaded_arg_count.
  // these counters are passed to the specifier parser function, which will track them internally
  // and then update the values through the pointers if the spec is valid.
  int arg_index = 0;
  int arg_count = 0;
  int loaded_arg_count = 0;
  int arg_sizes[FMT_MAX_ARGS] = {0};
  void *values[FMT_MAX_ARGS] = {0};

  // the following counter keeps track of specifiers when running in two-pass mode. in single-pass
  // mode specifiers are written directly to the buffer and so there is no limit on the number of
  // specifiers, as long as they dont reference more than FMT_MAX_ARGS arguments.
  int spec_index = 0;
  int pass_two_index;
  fmt_spec_t specs[FMT_MAX_SPECS] = {0};
  parsed_fmt_spec_t parsed_specs[FMT_MAX_SPECS] = {0};

  const char *ptr = format;
  const char *pass_two_start;
  while (*ptr && !fmt_buffer_full(&buf)) {
    // start of fmt specifier
    if (*ptr == '{') {
      if (*(ptr + 1) == '{') { // escaped
        if (single_pass)
          n += fmt_buffer_write_char(&buf, '{');

        ptr += 2;
        continue;
      }

      int cur_spec_index = spec_index;
      if (cur_spec_index >= FMT_MAX_SPECS)
        continue; // too many specifiers
      spec_index++;

      parsed_fmt_spec_t *parsed_spec = &parsed_specs[cur_spec_index];
      fmt_spec_t *spec = &specs[cur_spec_index];
      size_t m = parse_fmt_spec(ptr, max_args, &arg_index, &arg_count, parsed_spec);
      spec->end = ptr + m;
      ptr += m;
      if (!parsed_spec->valid)
        continue;

      if (single_pass && arg_count > arg_index + 1) {
        // the spec references an argument index greater than what we've loaded so far
        // so we have to switch to two-pass mode
        single_pass = false;
        pass_two_start = ptr - m;
        pass_two_index = cur_spec_index;
      }

      memcpy(spec->type, parsed_spec->type, min(parsed_spec->type_len, FMTLIB_MAX_TYPE_LEN));
      spec->type[parsed_spec->type_len] = 0;
      spec->type_len = parsed_spec->type_len;
      spec->value = NULL;
      spec->flags = parsed_spec->flags;
      spec->align = parsed_spec->align;
      spec->fill_char = parsed_spec->fill_char;

      // resolve specifier type
      int arg_size = fmtlib_resolve_type(spec);
      arg_sizes[parsed_spec->index] = arg_size;

      if (parsed_spec->width_is_index) {
        arg_sizes[parsed_spec->width_or_index] = sizeof(int);
      } else {
        spec->width = parsed_spec->width_or_index;
      }
      if (parsed_spec->precision_is_index) {
        arg_sizes[parsed_spec->precision_or_index] = sizeof(int);
      } else {
        spec->precision = parsed_spec->precision_or_index;
      }

      if (!single_pass) {
        continue;
      }

      // =======================
      // SINGLE-PASS
      if (arg_size < 0) {
        // invalid type
        n += fmt_buffer_write(&buf, "{bad type: ", 11);
        n += fmt_buffer_write(&buf, spec->type, parsed_spec->type_len);
        n += fmt_buffer_write_char(&buf, '}');
        continue;
      } else if (arg_size == 0) {
        // no value
        n += fmtlib_format(&buf, spec);
        continue;
      }

      // load argument(s)
      for (int i = loaded_arg_count; i < arg_count; i++) {
        values[i] = LOAD_ARG(i);
        loaded_arg_count++;
      }

      spec->value = values[parsed_spec->index];
      if (parsed_spec->width_is_index) {
        spec->width = *(int *)values[parsed_spec->width_or_index];
      }
      if (parsed_spec->precision_is_index) {
        spec->precision = *(int *)values[parsed_spec->precision_or_index];
      }

      // format
      n += fmtlib_format(&buf, spec);
    } else {
      if (single_pass)
        n += fmt_buffer_write_char(&buf, *ptr);

      ptr++;
    }
  }

  if (single_pass)
    return n;

  // =======================
  // DOUBLE-PASS

  // load argument(s)
  for (int i = loaded_arg_count; i < arg_count; i++) {
    values[i] = LOAD_ARG(i);
    loaded_arg_count++;
  }

  // now make a second pass over the format string to print it. this time we dont
  // have to
  ptr = pass_two_start;
  int index = pass_two_index;
  while (*ptr && !fmt_buffer_full(&buf) && index < spec_index) {
    if (*ptr == '{') {
      if (*(ptr + 1) == '{') { // escaped
        n += fmt_buffer_write_char(&buf, '{');
        ptr += 2;
        continue;
      }

      parsed_fmt_spec_t *parsed_spec = &parsed_specs[index];
      fmt_spec_t *spec = &specs[index];
      index++;
      if (!parsed_spec->valid)
        continue;

      spec->value = values[parsed_spec->index];
      if (parsed_spec->width_is_index) {
        spec->width = *(int *)values[parsed_spec->width_or_index];
      }
      if (parsed_spec->precision_is_index) {
        spec->precision = *(int *)values[parsed_spec->precision_or_index];
      }

      n += fmtlib_format(&buf, spec);
      ptr = spec->end;
    } else {
      n += fmt_buffer_write_char(&buf, *ptr);
      ptr++;
    }
  }

  while (*ptr && !fmt_buffer_full(&buf)) {
    n += fmt_buffer_write_char(&buf, *ptr);
    ptr++;
  }

  va_end(args_copy);
  return n;
#undef LOAD_ARG
}

#pragma clang diagnostic pop
