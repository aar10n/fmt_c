//
// Created by Aaron Gill-Braun on 2023-02-25.
//

#include "fmtlib.h"
#include <string.h>

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

#define TEMP_BUFFER_SIZE (FMTLIB_MAX_WIDTH + 1)

typedef struct fmt_format_type {
  const char *type;
  fmt_formatter_t fn;
  fmt_argtype_t argtype;
} fmt_format_type_t;

union double_raw {
  double value;
  struct {
    uint64_t frac : 52;
    uint64_t exp : 11;
    uint64_t sign : 1;
  };
};

struct num_format {
  int base;
  const char *digits;
  const char *prefix;
};

static const struct num_format binary_format = { .base = 2, .digits = "01", .prefix = "0b" };
static const struct num_format octal_format = { .base = 8, .digits = "01234567", .prefix = "0o" };
static const struct num_format decimal_format = { .base = 10, .digits = "0123456789", .prefix = "" };
static const struct num_format hex_lower_format = { .base = 16, .digits = "0123456789abcdef", .prefix = "0x" };
static const struct num_format hex_upper_format = { .base = 16, .digits = "0123456789ABCDEF", .prefix = "0X" };

static const double pow10[] = { 1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000 };

static fmt_format_type_t format_types[FMTLIB_MAX_TYPES];
static size_t num_format_types = 0;


static inline size_t u64_to_str(uint64_t value, char *buffer, const struct num_format *format) {
  size_t n = 0;
  int base = format->base;
  const char *digits = format->digits;
  if (value == 0) {
    buffer[n++] = '0';
    return n;
  }

  while (value > 0) {
    buffer[n++] = digits[value % base];
    value /= base;
  }

  // reverse buffer
  for (size_t i = 0; i < n / 2; i++) {
    char tmp = buffer[i];
    buffer[i] = buffer[n - i - 1];
    buffer[n - i - 1] = tmp;
  }
  return n;
}

// Writes a signed or unsigned number to the buffer using the given format.
static inline size_t format_integer(fmt_buffer_t *buffer, const fmt_spec_t *spec, bool is_signed, const struct num_format *format) {
  int width = min(max(spec->width, 0), FMTLIB_MAX_WIDTH);
  size_t n = 0;
  uint64_t v;
  bool is_negative = false;
  if (is_signed) {
    int64_t i = (int64_t) spec->value.uint64_value;
    if (i < 0) {
      v = -i;
      is_negative = true;
    } else {
      v = i;
    }
  } else {
    v = spec->value.uint64_value;
  }

  // write sign or space to buffer
  if (is_negative) {
    n += fmt_buffer_write_char(buffer, '-');
  } else if (spec->flags & FMT_FLAG_SIGN) {
    n += fmt_buffer_write_char(buffer, '+');
  } else if (spec->flags & FMT_FLAG_SPACE) {
    n += fmt_buffer_write_char(buffer, ' ');
  }

  // write prefix for alternate form (e.g. 0x) to buffer
  if (spec->flags & FMT_FLAG_ALT) {
    const char *ptr = format->prefix;
    while (*ptr) {
      n += fmt_buffer_write_char(buffer, *ptr++);
    }
  }

  // write digits to an intermediate buffer so we can calculate the
  // length of the number and apply precision and padding accordingly
  char temp[TEMP_BUFFER_SIZE];
  size_t len = u64_to_str(v, temp, format);

  // pad with leading zeros to reach specified precision
  if ((size_t)spec->precision > len) {
    size_t padding = spec->precision - len;
    for (size_t i = 0; i < padding; i++) {
      n += fmt_buffer_write_char(buffer, '0');
    }
  }

  // left-pad number with zeros to reach specified width
  if (spec->flags & FMT_FLAG_ZERO && (size_t)width > len + n) {
    // normally padding is handled outside of this function and is applied to the
    // entire number including the sign or prefix. however, when the zero flag is
    // set, the zero padding is applied to the number only and keeps the sign or
    // prefix in front of the number.
    if ((size_t)width > len + n) {
      size_t padding = width - len - n;
      for (size_t i = 0; i < padding; i++) {
        n += fmt_buffer_write_char(buffer, '0');
      }
    }
  }

  // finally write the number to the buffer
  n += fmt_buffer_write(buffer, temp, len);
  return n;
}

// Writes a floating point number to the buffer.
static inline size_t format_double(fmt_buffer_t *buffer, const fmt_spec_t *spec) {
  union double_raw v = { .value = spec->value.double_value };
  int width = min(max(spec->width, 0), FMTLIB_MAX_WIDTH);
  size_t n = 0;

  // write sign or space to buffer
  if (v.sign) {
    n += fmt_buffer_write_char(buffer, '-');
  } if (spec->flags & FMT_FLAG_SIGN) {
    n += fmt_buffer_write_char(buffer, '+');
  } else if (spec->flags & FMT_FLAG_SPACE) {
    n += fmt_buffer_write_char(buffer, ' ');
  }

  // handle special encodings
  if (v.exp == 0x7FF && v.frac == 0) {
    // infinity
    const char *inf = spec->flags & FMT_FLAG_UPPER ? "INF" : "inf";
    n += fmt_buffer_write(buffer, inf, 3);
    return n;
  } else if (v.exp == 0x7FF && v.frac != 0) {
    // NaN
    const char *nan = spec->flags & FMT_FLAG_UPPER ? "NAN" : "nan";
    n += fmt_buffer_write(buffer, nan, 3);
    return n;
  } else if (v.exp == 0 && v.frac == 0) {
    // zero
    n += fmt_buffer_write(buffer, "0", 1);
    return n;
  }

  if (v.value < 0) {
    v.value = -v.value;
  }

  // now to convert floating point numbers to strings we need to extract the whole
  // and fractional parts as integers. from there we simply convert each to a string
  // then write them to the buffer.

  // using a precision over 9 can lead to overflow errors
  int prec = spec->precision > 0 ? min(spec->precision, 9) : 6;
  uint64_t whole = (uint64_t) v.value;
  uint64_t frac;

  // shift the decimal point to the right by the specified precision
  double tmp = (v.value - (double)whole) * pow10[prec];
  frac = (uint64_t) tmp;

  // round the remaining fractional part
  double delta = tmp - (double)frac;
  if (delta > 0.5) {
    frac++;
    // handle rollover, e.g. case 0.99 with prec 1 is 1.0
    if (frac >= (uint64_t)pow10[prec]) {
      frac = 0;
      whole++;
    }
  } else if (delta < 0.5) {
    // do nothing
  } else if ((frac == 0) || (frac & 1)) {
    // if halfway, round up if odd or last digit is 0
    frac++;
  }

  // write the whole part to the intermediate buffer
  char temp[TEMP_BUFFER_SIZE];
  size_t len = u64_to_str(whole, temp, &decimal_format);
  temp[len++] = '.';
  // write the fractional part to the intermediate buffer
  size_t frac_len = u64_to_str(frac, temp + len, &decimal_format);
  len += frac_len;
  if ((size_t)spec->precision > frac_len) {
    // we have to factor precision padding into the length but we can't write it
    // to the buffer until after we've written the number.
    len += spec->precision - frac_len;
  }

  // left-pad number with zeros to reach specified width
  if (spec->flags & FMT_FLAG_ZERO && (size_t)width > len + n) {
    if ((size_t)width > len + n) {
      size_t padding = width - len - n;
      for (size_t i = 0; i < padding; i++) {
        n += fmt_buffer_write_char(buffer, '0');
      }
    }
  }

  // now write the number to the buffer
  n += fmt_buffer_write(buffer, temp, len);

  // finally write the trailing zeros to the buffer
  if ((size_t)spec->precision > frac_len) {
    size_t padding = spec->precision - frac_len;
    for (size_t i = 0; i < padding; i++) {
      n += fmt_buffer_write_char(buffer, '0');
    }
  }
  return n;
}

// aligns the string to the spec width
static inline size_t apply_alignment(fmt_buffer_t *buffer, const fmt_spec_t *spec, const char *str, size_t len) {
  if (len > (size_t)spec->width) {
    return fmt_buffer_write(buffer, str, len);
  }

  size_t n = 0;
  size_t padding = spec->width - len;
  char pad_char = spec->fill_char;
  switch (spec->align) {
    case FMT_ALIGN_LEFT:
      for (size_t i = 0; i < padding; i++) {
        n += fmt_buffer_write_char(buffer, pad_char);
      }
      n += fmt_buffer_write(buffer, str, len);
      break;
    case FMT_ALIGN_RIGHT:
      n += fmt_buffer_write(buffer, str, len);
      for (size_t i = 0; i < padding; i++) {
        n += fmt_buffer_write_char(buffer, pad_char);
      }
      break;
    case FMT_ALIGN_CENTER:
      for (size_t i = 0; i < padding / 2; i++) {
        n += fmt_buffer_write_char(buffer, pad_char);
      }
      n += fmt_buffer_write(buffer, str, len);
      for (size_t i = 0; i < padding - padding / 2; i++) {
        n += fmt_buffer_write_char(buffer, pad_char);
      }
      break;
  }
  return n;
}


// MARK: - Public API

size_t fmtlib_format_signed(fmt_buffer_t *buffer, const fmt_spec_t *spec) {
  char value_data[TEMP_BUFFER_SIZE];
  fmt_buffer_t value = fmt_buffer(value_data, TEMP_BUFFER_SIZE);

  size_t len = format_integer(&value, spec, true, &decimal_format);
  return apply_alignment(buffer, spec, value_data, len);
}

size_t fmtlib_format_unsigned(fmt_buffer_t *buffer, const fmt_spec_t *spec) {
  char value_data[TEMP_BUFFER_SIZE];
  fmt_buffer_t value = fmt_buffer(value_data, TEMP_BUFFER_SIZE);

  size_t len = format_integer(&value, spec, false, &decimal_format);
  return apply_alignment(buffer, spec, value_data, len);
}

size_t fmtlib_format_binary(fmt_buffer_t *buffer, const fmt_spec_t *spec) {
  char value_data[TEMP_BUFFER_SIZE];
  fmt_buffer_t value = fmt_buffer(value_data, TEMP_BUFFER_SIZE);

  size_t len = format_integer(&value, spec, false, &binary_format);
  return apply_alignment(buffer, spec, value_data, len);
}

size_t fmtlib_format_octal(fmt_buffer_t *buffer, const fmt_spec_t *spec) {
  char value_data[TEMP_BUFFER_SIZE];
  fmt_buffer_t value = fmt_buffer(value_data, TEMP_BUFFER_SIZE);

  size_t len = format_integer(&value, spec, false, &octal_format);
  return apply_alignment(buffer, spec, value_data, len);
}

size_t fmtlib_format_hex(fmt_buffer_t *buffer, const fmt_spec_t *spec) {
  const struct num_format *format = spec->flags & FMT_FLAG_UPPER ? &hex_upper_format : &hex_lower_format;
  char value_data[TEMP_BUFFER_SIZE];
  fmt_buffer_t value = fmt_buffer(value_data, TEMP_BUFFER_SIZE);

  size_t len = format_integer(&value, spec, false, format);
  return apply_alignment(buffer, spec, value_data, len);
}

size_t fmtlib_format_double(fmt_buffer_t *buffer, const fmt_spec_t *spec) {
  char value_data[TEMP_BUFFER_SIZE];
  fmt_buffer_t value = fmt_buffer(value_data, TEMP_BUFFER_SIZE);

  size_t len = format_double(&value, spec);
  return apply_alignment(buffer, spec, value_data, len);
}

size_t fmtlib_format_string(fmt_buffer_t *buffer, const fmt_spec_t *spec) {
  const char *str = spec->value.voidptr_value;
  size_t len = spec->precision;
  if (str == NULL) {
    str = "(null)";
    len = 6;
  } else if (len == 0) {
    len = strlen(str);
  }

  if (spec->width == 0) {
    return fmt_buffer_write(buffer, str, len);
  }

  // width specified, align the string
  return apply_alignment(buffer, spec, str, len);
}

size_t fmtlib_format_char(fmt_buffer_t *buffer, const fmt_spec_t *spec) {
  char c = *((char *)&spec->value);
  const char *str = &c;
  size_t len = 1;
  if (c == 0) {
    str = "\\0";
    len = 2;
  }

  if (spec->width == 0) {
    return fmt_buffer_write(buffer, str, len);
  }

  // width specified, align the char
  return apply_alignment(buffer, spec, str, len);
}

int fmtlib_atoi(const char *data, size_t size) {
  int value = 0;
  int sign = 1;
  if (data[0] == '-') {
    sign = -1;
    data++;
    size--;
  }

  for (size_t i = 0; i < size; i++) {
    value *= 10;
    value += data[i] - '0';
  }
  return sign * value;
}

//

void fmtlib_register_type(const char *type, fmt_formatter_t fn, fmt_argtype_t argtype) {
  if (num_format_types >= FMTLIB_MAX_TYPES) {
    return;
  }

  format_types[num_format_types].type = type;
  format_types[num_format_types].fn = fn;
  format_types[num_format_types].argtype = argtype;
  num_format_types++;
}

int fmtlib_resolve_type(fmt_spec_t *spec) {
  const char *type = spec->type;
  if (spec->type_len == 0) {
    spec->argtype = FMT_ARGTYPE_NONE;
    spec->formatter = NULL;
    return 1;
  } else if (spec->type_len == 1) {
    switch (spec->type[0]) {
      case 'd':
        spec->argtype = FMT_ARGTYPE_INT32;
        spec->formatter = fmtlib_format_signed;
        return 1;
      case 'u':
        spec->argtype = FMT_ARGTYPE_UINT32;
        spec->formatter = fmtlib_format_unsigned;
        return 1;
      case 'b':
        spec->argtype = FMT_ARGTYPE_UINT32;
        spec->formatter = fmtlib_format_binary;
        return 1;
      case 'o':
        spec->argtype = FMT_ARGTYPE_UINT32;
        spec->formatter = fmtlib_format_octal;
        return 1;
      case 'X':
        spec->flags |= FMT_FLAG_UPPER;
        // fallthrough
      case 'x':
        spec->argtype = FMT_ARGTYPE_UINT32;
        spec->formatter = fmtlib_format_hex;
        return 1;
      case 'F':
        spec->flags |= FMT_FLAG_UPPER;
        // fallthrough
      case 'f':
        spec->argtype = FMT_ARGTYPE_DOUBLE;
        spec->formatter = fmtlib_format_double;
        return 1;
      case 's':
        spec->argtype = FMT_ARGTYPE_VOIDPTR;
        spec->formatter = fmtlib_format_string;
        return 1;
      case 'c':
        spec->argtype = FMT_ARGTYPE_INT32;
        spec->formatter = fmtlib_format_char;
        return 1;
      default:
        break;
    }
  } else if (spec->type_len == 3) {
    if (strncmp(type, "lld", 3) == 0) {
      spec->argtype = FMT_ARGTYPE_INT32;
      spec->formatter = fmtlib_format_signed;
      return 1;
    } else if (strncmp(type, "llu", 3) == 0) {
      spec->argtype = FMT_ARGTYPE_UINT64;
      spec->formatter = fmtlib_format_unsigned;
      return 1;
    } else if (strncmp(type, "llb", 3) == 0) {
      spec->argtype = FMT_ARGTYPE_UINT64;
      spec->formatter = fmtlib_format_binary;
      return 1;
    } else if (strncmp(type, "llo", 3) == 0) {
      spec->argtype = FMT_ARGTYPE_UINT64;
      spec->formatter = fmtlib_format_octal;
      return 1;
    } else if (strncmp(type, "llX", 3) == 0) {
      spec->flags |= FMT_FLAG_UPPER;
      spec->argtype = FMT_ARGTYPE_UINT64;
      spec->formatter = fmtlib_format_hex;
      return 1;
    } else if (strncmp(type, "llx", 3) == 0) {
      spec->argtype = FMT_ARGTYPE_UINT64;
      spec->formatter = fmtlib_format_hex;
      return 1;
    }
  }

  // TODO: maybe use something faster here?
  for (size_t i = 0; i < num_format_types; i++) {
    if (strcmp(format_types[i].type, type) == 0) {
      spec->argtype = format_types[i].argtype;
      spec->formatter = format_types[i].fn;
      return format_types[i].argtype;
    }
  }

  // type not found
  spec->argtype = FMT_ARGTYPE_NONE;
  spec->formatter = NULL;
  return 0;
}

size_t fmtlib_format(fmt_buffer_t *buffer, fmt_spec_t *spec) {
  if (spec->type_len == 0) {
    // no type specified, just apply alignment/padding
    return apply_alignment(buffer, spec, "", 0);
  } else if (spec->formatter == NULL) {
    return 0;
  }

  return spec->formatter(buffer, spec);
}
