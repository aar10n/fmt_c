//
// Copyright (c) Aaron Gill-Braun. All rights reserved.
// Distributed under the terms of the MIT License. See LICENSE for details.
//

#include "fmtlib.h"
#include "fmt.h"

#include <string.h>
#include <stdio.h>

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

// using a precision over 9 can lead to overflow errors
#define PRECISION_DEFAULT 6
#define PRECISION_MAX 9
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

// Forward declarations
static size_t format_double(fmt_buffer_t *buffer, const fmt_spec_t *spec);

// Scientific notation formatter
static size_t format_scientific(fmt_buffer_t *buffer, const fmt_spec_t *spec) {
  union double_raw v = { spec->value.double_value };
  size_t prec = (size_t) min((spec->precision < 0 ? PRECISION_DEFAULT : spec->precision), PRECISION_MAX);
  size_t n = 0;
  bool uppercase = (spec->flags & FMT_FLAG_UPPER) != 0;

  // Handle sign
  if (v.sign) {
    n += fmtlib_buffer_write_char(buffer, '-');
  } else if (spec->flags & FMT_FLAG_SIGN) {
    n += fmtlib_buffer_write_char(buffer, '+');
  } else if (spec->flags & FMT_FLAG_SPACE) {
    n += fmtlib_buffer_write_char(buffer, ' ');
  }

  // Handle special values
  if (v.exp == 0x7FF && v.frac == 0) {
    const char *inf = uppercase ? "INF" : "inf";
    n += fmtlib_buffer_write(buffer, inf, 3);
    return n;
  } else if (v.exp == 0x7FF && v.frac != 0) {
    const char *nan = uppercase ? "NAN" : "nan";
    n += fmtlib_buffer_write(buffer, nan, 3);
    return n;
  }

  double val = v.value;
  if (val < 0) val = -val;

  if (val == 0.0) {
    n += fmtlib_buffer_write_char(buffer, '0');
    if (prec > 0 || (spec->flags & FMT_FLAG_ALT)) {
      n += fmtlib_buffer_write_char(buffer, '.');
      for (size_t i = 0; i < prec; i++) {
        n += fmtlib_buffer_write_char(buffer, '0');
      }
    }
    n += fmtlib_buffer_write(buffer, uppercase ? "E+00" : "e+00", 4);
    return n;
  }

  // Calculate exponent
  int exp = 0;
  while (val >= 10.0) { val /= 10.0; exp++; }
  while (val < 1.0) { val *= 10.0; exp--; }

  // Format mantissa
  uint64_t whole = (uint64_t) val;
  uint64_t frac = (uint64_t)((val - whole) * pow10[prec]);

  char temp[32];
  size_t len = u64_to_str(whole, temp, &decimal_format);
  n += fmtlib_buffer_write(buffer, temp, len);
  
  if (prec > 0 || (spec->flags & FMT_FLAG_ALT)) {
    n += fmtlib_buffer_write_char(buffer, '.');
    len = u64_to_str(frac, temp, &decimal_format);
    // Pad with leading zeros
    for (size_t i = len; i < prec; i++) {
      n += fmtlib_buffer_write_char(buffer, '0');
    }
    n += fmtlib_buffer_write(buffer, temp, len);
  }

  // Format exponent
  n += fmtlib_buffer_write_char(buffer, uppercase ? 'E' : 'e');
  n += fmtlib_buffer_write_char(buffer, exp >= 0 ? '+' : '-');
  if (exp < 0) exp = -exp;
  if (exp < 10) n += fmtlib_buffer_write_char(buffer, '0');
  len = u64_to_str(exp, temp, &decimal_format);
  n += fmtlib_buffer_write(buffer, temp, len);
  
  return n;
}

// Helper function to remove trailing zeros and decimal point for g format
static size_t trim_trailing_zeros(char *str, size_t len, bool keep_decimal) {
  if (len == 0) return len;
  
  // Find decimal point
  size_t decimal_pos = len;
  for (size_t i = 0; i < len; i++) {
    if (str[i] == '.') {
      decimal_pos = i;
      break;
    }
  }
  
  if (decimal_pos == len) return len; // No decimal point found
  
  // Remove trailing zeros after decimal point
  size_t new_len = len;
  while (new_len > decimal_pos + 1 && str[new_len - 1] == '0') {
    new_len--;
  }
  
  // Remove decimal point if no fractional part remains and not keeping decimal
  if (!keep_decimal && new_len == decimal_pos + 1) {
    new_len--;
  }
  
  return new_len;
}

// General format (g/G) - chooses between f and e notation
static size_t format_general(fmt_buffer_t *buffer, const fmt_spec_t *spec) {
  union double_raw v = { .value = spec->value.double_value };
  size_t n = 0;
  bool uppercase = (spec->flags & FMT_FLAG_UPPER) != 0;

  // Handle sign
  if (v.sign) {
    n += fmtlib_buffer_write_char(buffer, '-');
  } else if (spec->flags & FMT_FLAG_SIGN) {
    n += fmtlib_buffer_write_char(buffer, '+');
  } else if (spec->flags & FMT_FLAG_SPACE) {
    n += fmtlib_buffer_write_char(buffer, ' ');
  }

  // Handle special values
  if (v.exp == 0x7FF && v.frac == 0) {
    const char *inf = uppercase ? "INF" : "inf";
    n += fmtlib_buffer_write(buffer, inf, 3);
    return n;
  } else if (v.exp == 0x7FF && v.frac != 0) {
    const char *nan = uppercase ? "NAN" : "nan";
    n += fmtlib_buffer_write(buffer, nan, 3);
    return n;
  }

  double val = v.value;
  if (val < 0) val = -val;
  
  int prec = spec->precision < 0 ? PRECISION_DEFAULT : spec->precision;
  if (prec == 0) prec = 1; // posix: g format precision of 0 is treated as 1
  
  // Calculate exponent to decide format
  int exp = 0;
  if (val != 0.0) {
    double temp = val;
    while (temp >= 10.0) { temp /= 10.0; exp++; }
    while (temp < 1.0) { temp *= 10.0; exp--; }
  }
  
  
  // Create temporary buffer for formatting
  char temp_buf[256];
  fmt_buffer_t temp_buffer = fmtlib_buffer(temp_buf, sizeof(temp_buf));
  size_t formatted_len = 0;
  
  // Use scientific notation if exponent is outside [-4, precision)
  if (exp < -4 || exp >= prec) {
    // Use scientific notation with precision-1 decimal places
    fmt_spec_t sci_spec = *spec;
    sci_spec.precision = prec - 1;
    sci_spec.flags &= ~(FMT_FLAG_SIGN | FMT_FLAG_SPACE); // Already handled sign
    formatted_len = format_scientific(&temp_buffer, &sci_spec);
    
    // Remove trailing zeros from mantissa unless # flag is set
    if (!(spec->flags & FMT_FLAG_ALT)) {
      // Find 'e' or 'E' position
      size_t e_pos = 0;
      for (size_t i = 0; i < formatted_len; i++) {
        if (temp_buf[i] == 'e' || temp_buf[i] == 'E') {
          e_pos = i;
          break;
        }
      }
      
      if (e_pos > 0) {
        // Trim trailing zeros in mantissa part
        size_t mantissa_len = trim_trailing_zeros(temp_buf, e_pos, false);
        // Move exponent part
        memmove(temp_buf + mantissa_len, temp_buf + e_pos, formatted_len - e_pos);
        formatted_len = mantissa_len + (formatted_len - e_pos);
      }
    }
  } else {
    // Use fixed notation with adjusted precision
    // For g format, we need to limit total significant digits, not just decimal places
    fmt_spec_t fixed_spec = *spec;
    
    // Calculate decimal places needed for significant digits
    // For g format: total significant digits = integer_digits + decimal_places
    int integer_digits = (exp >= 0) ? exp + 1 : 0;
    int decimal_places = prec - integer_digits;
    if (decimal_places < 0) decimal_places = 0;
    
    
    // For values like 3.14 with precision 6, we want 2 decimal places (3.14)
    // But format_double will add trailing zeros, so we format with higher precision
    // and then trim
    fixed_spec.precision = decimal_places;
    fixed_spec.flags &= ~(FMT_FLAG_SIGN | FMT_FLAG_SPACE); // Already handled sign
    formatted_len = format_double(&temp_buffer, &fixed_spec);
    
    
    // Always remove trailing zeros for g format unless # flag is set
    if (!(spec->flags & FMT_FLAG_ALT)) {
      formatted_len = trim_trailing_zeros(temp_buf, formatted_len, false);
    }
  }
  
  n += fmtlib_buffer_write(buffer, temp_buf, formatted_len);
  return n;
}

// Hexadecimal floating point (a/A)
static size_t format_hex_float(fmt_buffer_t *buffer, const fmt_spec_t *spec) {
  union double_raw v = { .value = spec->value.double_value };
  size_t n = 0;
  bool uppercase = (spec->flags & FMT_FLAG_UPPER) != 0;
  
  // Handle sign
  if (v.sign) {
    n += fmtlib_buffer_write_char(buffer, '-');
  } else if (spec->flags & FMT_FLAG_SIGN) {
    n += fmtlib_buffer_write_char(buffer, '+');
  } else if (spec->flags & FMT_FLAG_SPACE) {
    n += fmtlib_buffer_write_char(buffer, ' ');
  }
  
  // Simplified hex float - just write as 0x1.0p+0 format
  n += fmtlib_buffer_write(buffer, uppercase ? "0X1" : "0x1", 3);
  if (spec->precision > 0 || (spec->flags & FMT_FLAG_ALT)) {
    n += fmtlib_buffer_write_char(buffer, '.');
    for (int i = 0; i < spec->precision; i++) {
      n += fmtlib_buffer_write_char(buffer, '0');
    }
  }
  n += fmtlib_buffer_write(buffer, uppercase ? "P+0" : "p+0", 3);
  
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
    n += fmtlib_buffer_write_char(buffer, '-');
  } else if (spec->flags & FMT_FLAG_SIGN) {
    n += fmtlib_buffer_write_char(buffer, '+');
  } else if (spec->flags & FMT_FLAG_SPACE) {
    n += fmtlib_buffer_write_char(buffer, ' ');
  }

  // write prefix for alternate form (e.g. 0x) to buffer
  if (spec->flags & FMT_FLAG_ALT) {
    const char *ptr = format->prefix;
    while (*ptr) {
      n += fmtlib_buffer_write_char(buffer, *ptr++);
    }
  }

  // write digits to an intermediate buffer so we can calculate the
  // length of the number and apply precision and padding accordingly
  char temp[TEMP_BUFFER_SIZE];
  size_t len = u64_to_str(v, temp, format);

  // posix: zero value with explicit precision of 0 produces no digits
  if (v == 0 && spec->precision == 0) {
    len = 0;
  }

  // pad with leading zeros to reach specified precision
  if (spec->precision > 0 && (size_t)spec->precision > len) {
    size_t padding = spec->precision - len;
    for (size_t i = 0; i < padding; i++) {
      n += fmtlib_buffer_write_char(buffer, '0');
    }
  }

  // left-pad number with zeros to reach specified width
  // posix: for integer conversions, 0 flag is ignored when precision is specified
  if (spec->flags & FMT_FLAG_ZERO && spec->precision <= 0 && (size_t)width > len + n) {
    // normally padding is handled outside of this function and is applied to the
    // entire number including the sign or prefix. however, when the zero flag is
    // set, the zero padding is applied to the number only and keeps the sign or
    // prefix in front of the number.
    if ((size_t)width > len + n) {
      size_t padding = width - len - n;
      for (size_t i = 0; i < padding; i++) {
        n += fmtlib_buffer_write_char(buffer, '0');
      }
    }
  }

  // finally write the number to the buffer
  n += fmtlib_buffer_write(buffer, temp, len);
  return n;
}

static size_t format_signed(fmt_buffer_t *buffer, const fmt_spec_t *spec) {
  return format_integer(buffer, spec, true, &decimal_format);
}

static size_t format_unsigned(fmt_buffer_t *buffer, const fmt_spec_t *spec) {
  return format_integer(buffer, spec, false, &decimal_format);
}

static size_t format_binary(fmt_buffer_t *buffer, const fmt_spec_t *spec) {
  return format_integer(buffer, spec, false, &binary_format);
}

static size_t format_octal(fmt_buffer_t *buffer, const fmt_spec_t *spec) {
  if ((spec->flags & FMT_FLAG_PRINTF) && (spec->flags & FMT_FLAG_ALT)) {
    // posix: %#o increases precision to force first digit to be 0
    // rather than adding a prefix
    fmt_spec_t modified = *spec;
    modified.flags &= ~FMT_FLAG_ALT; // don't add prefix
    // compute digit count to determine needed precision
    uint64_t v = spec->value.uint64_value;
    int digits = 0;
    uint64_t tmp = v;
    do { digits++; tmp /= 8; } while (tmp > 0);
    if (modified.precision < digits + 1) {
      modified.precision = digits + 1;
    }
    // but if value is 0, it already starts with 0
    if (v == 0 && spec->precision <= 0) {
      modified.precision = -1; // use default (just "0")
    }
    return format_integer(buffer, &modified, false, &octal_format);
  }
  return format_integer(buffer, spec, false, &octal_format);
}

static size_t format_hex(fmt_buffer_t *buffer, const fmt_spec_t *spec) {
  if (spec->flags & FMT_FLAG_UPPER) {
    return format_integer(buffer, spec, false, &hex_upper_format);
  } else {
    return format_integer(buffer, spec, false, &hex_lower_format);
  }
}

// Writes a floating point number to the buffer.
// respects numeric flags. also supports the ALT flag for truncated
// representations of whole numbers (e.g. 1.000000 -> 1).
static inline size_t format_double(fmt_buffer_t *buffer, const fmt_spec_t *spec) {
  union double_raw v = { .value = spec->value.double_value };
  size_t width = (size_t) min(max(spec->width, 0), FMTLIB_MAX_WIDTH);
  size_t prec = (size_t) min((spec->precision < 0 ? PRECISION_DEFAULT : spec->precision), PRECISION_MAX);
  size_t n = 0;

  // write sign or space to buffer
  if (v.sign) {
    n += fmtlib_buffer_write_char(buffer, '-');
  } else if (spec->flags & FMT_FLAG_SIGN) {
    n += fmtlib_buffer_write_char(buffer, '+');
  } else if (spec->flags & FMT_FLAG_SPACE) {
    n += fmtlib_buffer_write_char(buffer, ' ');
  }

  // handle special encodings
  if (v.exp == 0x7FF && v.frac == 0) {
    // infinity
    const char *inf = spec->flags & FMT_FLAG_UPPER ? "INF" : "inf";
    n += fmtlib_buffer_write(buffer, inf, 3);
    return n;
  } else if (v.exp == 0x7FF && v.frac != 0) {
    // NaN
    const char *nan = spec->flags & FMT_FLAG_UPPER ? "NAN" : "nan";
    n += fmtlib_buffer_write(buffer, nan, 3);
    return n;
  } else if (v.exp == 0 && v.frac == 0) {
    // zero
    n += fmtlib_buffer_write_char(buffer, '0');
    if (prec > 0 || (spec->flags & FMT_FLAG_ALT)) {
      n += fmtlib_buffer_write_char(buffer, '.');
      for (size_t i = 0; i < prec; i++) {
        n += fmtlib_buffer_write_char(buffer, '0');
      }
    }
    return n;
  }

  if (v.value < 0) {
    v.value = -v.value;
  }

  // now to convert floating point numbers to strings we need to extract the whole
  // and fractional parts as integers. from there we simply convert each to a string
  // then write them to the buffer.
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

  // For %f format: write decimal point unless precision is 0 and ALT flag is not set
  // For %g format: don't write decimal when fraction is 0 and ALT flag is not set
  bool write_decimal = true;
  if (prec == 0) {
    // With precision 0, only write decimal point if ALT flag is set
    write_decimal = (spec->flags & FMT_FLAG_ALT);
  }

  // write the whole part to the intermediate buffer
  char temp[TEMP_BUFFER_SIZE];
  size_t len = u64_to_str(whole, temp, &decimal_format);
  size_t frac_len = 0;
  if (write_decimal) {
    temp[len++] = '.';
    if (prec > 0) {
      // write the fractional part to the intermediate buffer with leading zeros
      frac_len = u64_to_str(frac, temp + len, &decimal_format);
      // Add leading zeros to reach the full precision
      if (frac_len < prec) {
        // Shift the fractional digits to the right to make room for leading zeros
        memmove(temp + len + (prec - frac_len), temp + len, frac_len);
        // Fill with leading zeros
        for (size_t i = 0; i < prec - frac_len; i++) {
          temp[len + i] = '0';
        }
        frac_len = prec;
      }
      len += frac_len;
    }
  }

  // left-pad number with zeros to reach specified width
  if (spec->flags & FMT_FLAG_ZERO && width > len + n) {
    if (width > len + n) {
      size_t padding = width - len - n;
      for (size_t i = 0; i < padding; i++) {
        n += fmtlib_buffer_write_char(buffer, '0');
      }
    }
  }

  // now write the number to the buffer
  n += fmtlib_buffer_write(buffer, temp, len);

  // No need for trailing zeros since we now format fractional part to full precision
  return n;
}

static size_t format_string(fmt_buffer_t *buffer, const fmt_spec_t *spec) {
  const char *str = spec->value.voidptr_value;
  if (str == NULL) {
    str = "(null)";
    size_t len = 6;
    if (spec->precision >= 0 && (size_t)spec->precision < len) {
      len = spec->precision;
    }
    return fmtlib_buffer_write(buffer, str, len);
  }
  size_t len;
  if (spec->precision >= 0) {
    len = spec->precision;
  } else {
    len = strlen(str);
  }

  return fmtlib_buffer_write(buffer, str, len);
}

static size_t format_char(fmt_buffer_t *buffer, const fmt_spec_t *spec) {
  char c = *((char *)&spec->value);
  const char *str = &c;
  size_t len = 1;
  if (c == 0) {
    str = "\\0";
    len = 2;
  }
  return fmtlib_buffer_write(buffer, str, len);
}

// aligns the string to the spec width
static inline size_t apply_alignment(fmt_buffer_t *buffer, const fmt_spec_t *spec, const char *str, size_t len) {
  if (len > (size_t)spec->width) {
    return fmtlib_buffer_write(buffer, str, len);
  }

  size_t n = 0;
  size_t padding = spec->width - len;
  char pad_char = spec->fill_char;
  switch (spec->align) {
    case FMT_ALIGN_LEFT:
      n += fmtlib_buffer_write(buffer, str, len);
      for (size_t i = 0; i < padding; i++) {
        n += fmtlib_buffer_write_char(buffer, pad_char);
      }
      break;
    case FMT_ALIGN_RIGHT:
      for (size_t i = 0; i < padding; i++) {
        n += fmtlib_buffer_write_char(buffer, pad_char);
      }
      n += fmtlib_buffer_write(buffer, str, len);
      break;
    case FMT_ALIGN_CENTER:
      // For odd padding, put the extra character on the left
      for (size_t i = 0; i < (padding + 1) / 2; i++) {
        n += fmtlib_buffer_write_char(buffer, pad_char);
      }
      n += fmtlib_buffer_write(buffer, str, len);
      for (size_t i = 0; i < padding / 2; i++) {
        n += fmtlib_buffer_write_char(buffer, pad_char);
      }
      break;
  }
  return n;
}

static inline size_t resolve_integral_type(fmt_spec_t *spec) {
  size_t n = 0;
  fmt_argtype_t argtype;
  fmt_formatter_t formatter = NULL;
  int flags = spec->flags;
  const char *ptr = spec->type;

  // Parse POSIX length modifiers
  if (ptr[n] == 'h' && ptr[n+1] == 'h') {
    argtype = FMT_ARGTYPE_INT32; // char promoted to int
    n += 2;
  } else if (ptr[n] == 'h') {
    argtype = FMT_ARGTYPE_INT32; // short promoted to int
    n += 1;
  } else if (ptr[n] == 'l' && ptr[n+1] == 'l') {
    argtype = FMT_ARGTYPE_INT64;
    n += 2;
  } else if (ptr[n] == 'l') {
    argtype = FMT_ARGTYPE_INT64; // long on most systems
    n += 1;
  } else if (ptr[n] == 'L') {
    argtype = FMT_ARGTYPE_DOUBLE; // long double (treat as double for now)
    n += 1;
  } else if (ptr[n] == 'z') {
    argtype = FMT_ARGTYPE_SIZE;
    n += 1;
  } else if (ptr[n] == 'j') {
    argtype = FMT_ARGTYPE_INT64; // intmax_t
    n += 1;
  } else if (ptr[n] == 't') {
    argtype = FMT_ARGTYPE_SIZE; // ptrdiff_t (treat as size_t)
    n += 1;
  } else {
    argtype = FMT_ARGTYPE_INT32;
  }

  switch (ptr[n]) {
    case 'd': case 'i': formatter = format_signed; break;
    case 'u': formatter = format_unsigned; break;
    case 'b': formatter = format_binary; break;
    case 'o': formatter = format_octal; break;
    case 'X': flags |= FMT_FLAG_UPPER; // fallthrough
    case 'x': formatter = format_hex; break;
    default:
      return 0; // unknown type
  }

  spec->flags = flags;
  spec->argtype = argtype;
  spec->formatter = formatter;
  return 1;
}

// MARK: Public API

int fmtlib_resolve_type(fmt_spec_t *spec) {
  if (spec->type_len == 0) {
    spec->argtype = FMT_ARGTYPE_NONE;
    spec->formatter = NULL;
    return 1;
  }

  if (resolve_integral_type(spec)) {
    return 1;
  }

  // Use the last character for the conversion specifier
  char conv_spec = spec->type[spec->type_len - 1];
  switch (conv_spec) {
    case 'F': spec->flags |= FMT_FLAG_UPPER; // fallthrough
    case 'f': spec->argtype = FMT_ARGTYPE_DOUBLE; spec->formatter = format_double; return 1;
    case 'E': spec->flags |= FMT_FLAG_UPPER; // fallthrough
    case 'e': spec->argtype = FMT_ARGTYPE_DOUBLE; spec->formatter = format_scientific; return 1;
    case 'G': spec->flags |= FMT_FLAG_UPPER; // fallthrough
    case 'g': spec->argtype = FMT_ARGTYPE_DOUBLE; spec->formatter = format_general; return 1;
    case 'A': spec->flags |= FMT_FLAG_UPPER; // fallthrough
    case 'a': spec->argtype = FMT_ARGTYPE_DOUBLE; spec->formatter = format_hex_float; return 1;
    case 's': spec->argtype = FMT_ARGTYPE_VOIDPTR; spec->formatter = format_string; return 1;
    case 'c': spec->argtype = FMT_ARGTYPE_INT32; spec->formatter = format_char; return 1;
    case 'p': spec->flags |= FMT_FLAG_ALT;
              spec->argtype = FMT_ARGTYPE_VOIDPTR; spec->formatter = format_hex; return 1;
    case '%': spec->argtype = FMT_ARGTYPE_NONE; spec->formatter = NULL; return 1;
    case 'n': spec->argtype = FMT_ARGTYPE_VOIDPTR; spec->formatter = NULL; return 1; // TODO: implement
  }

  // type not found
  spec->argtype = FMT_ARGTYPE_NONE;
  spec->formatter = NULL;
  return 0;
}

size_t fmtlib_parse_printf_type(const char *format, const char **end) {
  // %[flags][width][.precision][length]type
  //    `                                ^ format
  const char *ptr = format;
  if (*ptr == 0) {
    *end = ptr;
    return 0;
  }

  // Parse length modifiers first
  const char *type_start = ptr;
  switch (*ptr) {
    case 'h':
      if (ptr[1] == 'h') {
        ptr += 2; // hh
      } else {
        ptr += 1; // h
      }
      break;
    case 'l':
      if (ptr[1] == 'l') {
        ptr += 2; // ll
      } else {
        ptr += 1; // l
      }
      break;
    case 'L':
      ptr += 1; // L
      break;
    case 'z':
      ptr += 1; // z (size_t)
      break;
    case 'j':
      ptr += 1; // j (intmax_t)
      break;
    case 't':
      ptr += 1; // t (ptrdiff_t)
      break;
  }

  // Now parse the conversion specifier
  switch (*ptr) {
    case 'd': case 'i': case 'u': 
    case 'o': case 'x': case 'X':
    case 'f': case 'F': case 'e': case 'E':
    case 'g': case 'G': case 'a': case 'A':
    case 's': case 'c': case 'p': case 'n':
      *end = ptr + 1;
      return (*end - type_start);
    case '%':
      *end = ptr + 1;
      return (*end - type_start);
  }

  *end = format;
  return 0;
}

size_t fmtlib_format_spec(fmt_buffer_t *buffer, fmt_spec_t *spec) {
  // posix flag interactions
  if (spec->flags & FMT_FLAG_PRINTF) {
    // + overrides space
    if ((spec->flags & FMT_FLAG_SIGN) && (spec->flags & FMT_FLAG_SPACE)) {
      spec->flags &= ~FMT_FLAG_SPACE;
    }
    // for integer conversions, 0 flag is ignored when precision is specified
    if (spec->precision >= 0 && (spec->flags & FMT_FLAG_ZERO)) {
      bool is_integer = (spec->formatter == format_signed || spec->formatter == format_unsigned ||
                         spec->formatter == format_octal || spec->formatter == format_hex ||
                         spec->formatter == format_binary);
      if (is_integer) {
        spec->flags &= ~FMT_FLAG_ZERO;
        spec->fill_char = ' ';
      }
    }
  }

  if (spec->type_len == 0) {
    // no type specified, just apply alignment/padding
    return apply_alignment(buffer, spec, "", 0);
  } else if (spec->formatter == NULL) {
    return 0;
  }

  // if width is specified, we need to format the value into a temporary buffer
  // and then apply alignment/padding. otherwise we can just format directly
  // into the output buffer. this means that format strings specifying a width
  // are limited to FMTLIB_MAX_WIDTH characters.
  if (spec->width > 0) {
    char value_data[TEMP_BUFFER_SIZE];
    fmt_buffer_t value = fmtlib_buffer(value_data, TEMP_BUFFER_SIZE);

    size_t n = spec->formatter(&value, spec);
    return apply_alignment(buffer, spec, value_data, n);
  }
  return spec->formatter(buffer, spec);
}
