//
// Created by Aaron Gill-Braun on 2023-02-25.
//

#ifndef LIB_FMT_FMTLIB_H
#define LIB_FMT_FMTLIB_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>

// Custom Specifier Types
// ==========================
// fmtlib provides a way to add support for new specifier types at runtime
// by registering formatter functions. To add a new type, simply call the
// `fmt_register_type` function with your unique type name, the size of that
// type in bytes, and the formatter function.
//
// The formatter function should use the fmt_buffer_ functions when writing
// to the buffer, and it should return the number of bytes actually written.

// determines the maximum width that can be specified
// this should be large enough to handle any reasonable use case
#define FMTLIB_MAX_WIDTH 256

// determines the maximum number of user defined specifier types that can be registered
#define FMTLIB_MAX_TYPES 128

// determines the maximum allowed length of a specifier type name.
#define FMTLIB_MAX_TYPE_LEN 16

// -----------------------------------------------------------------------------

#define FMT_FLAG_ALT    0x01 // alternate form
#define FMT_FLAG_UPPER  0x02 // uppercase form
#define FMT_FLAG_SIGN   0x04 // always print sign for numeric values
#define FMT_FLAG_SPACE  0x08 // leave a space in front of positive numeric values
#define FMT_FLAG_ZERO   0x10 // pad to width with leading zeros and keeps sign in front

typedef enum fmt_align {
  FMT_ALIGN_LEFT,
  FMT_ALIGN_CENTER,
  FMT_ALIGN_RIGHT,
} fmt_align_t;

typedef struct fmt_spec fmt_spec_t;
typedef struct fmt_buffer fmt_buffer_t;

/// A function which writes a string to the buffer formatted according to the given specifier.
typedef size_t (*fmt_formatter_t)(fmt_buffer_t *buffer, const fmt_spec_t *spec);

/// Represents a fully-formed format specifier.
typedef struct fmt_spec {
  char type[FMTLIB_MAX_TYPE_LEN + 1];
  size_t type_len;
  int flags;
  int width;
  int precision;
  fmt_align_t align;
  char fill_char;
  const char *end;
  //
  void *value;
  fmt_formatter_t formatter;
} fmt_spec_t;

// MARK: fmt_buffer_t API
// ======================
// This simple struct is used to safely bounds-check all writes to the buffer.

typedef struct fmt_buffer {
  char *data;
  size_t size;
  size_t written;
} fmt_buffer_t;

static inline fmt_buffer_t fmt_buffer(char *data, size_t size) {
  memset(data, 0, size);
  return (fmt_buffer_t) {
    .data = data,
    .size = size - 1, // null terminator
  };
}

static inline bool fmt_buffer_full(fmt_buffer_t *b) {
  return b->size == 0;
}

static inline size_t fmt_buffer_write(fmt_buffer_t *b, const char *data, size_t size) {
  if (b->size == 0)
    return 0;
  size_t n = size < b->size ? size : b->size;
  memcpy(b->data, data, n);
  b->data += n;
  b->size -= n;
  b->written += n;
  return n;
}

static inline size_t fmt_buffer_write_char(fmt_buffer_t *b, char c) {
  if (b->size == 0)
    return 0;
  *b->data = c;
  b->data++;
  b->size--;
  b->written++;
  return 1;
}

// -----------------------------------------------------------------------------

size_t fmtlib_format_signed(fmt_buffer_t *buffer, const fmt_spec_t *spec);
size_t fmtlib_format_unsigned(fmt_buffer_t *buffer, const fmt_spec_t *spec);
size_t fmtlib_format_binary(fmt_buffer_t *buffer, const fmt_spec_t *spec);
size_t fmtlib_format_octal(fmt_buffer_t *buffer, const fmt_spec_t *spec);
size_t fmtlib_format_hex(fmt_buffer_t *buffer, const fmt_spec_t *spec);
size_t fmtlib_format_double(fmt_buffer_t *buffer, const fmt_spec_t *spec);

size_t fmtlib_format_string(fmt_buffer_t *buffer, const fmt_spec_t *spec);
size_t fmtlib_format_char(fmt_buffer_t *buffer, const fmt_spec_t *spec);
size_t fmtlib_format_type(fmt_buffer_t *buffer, const fmt_spec_t *spec, const char *type);

int fmtlib_atoi(const char *data, size_t size);

/**
 * Registers a new format specifier type.
 *
 * @param type The type name
 * @param size The byte size of this type
 * @param fn The function which formats this type
 */
void fmtlib_register_type(const char *type, int size, fmt_formatter_t fn);

/**
 * Resolves the specifier type to a formatter function and type size.
 * If the type exists, spec->formatter will be set and the function will
 * return the size of the type.
 *
 * @param type The type name
 * @param spec The format specifier
 * @return The size of the type in bytes or -1 if the type is unknown
 */
int fmtlib_resolve_type(fmt_spec_t *spec);

/**
 * Formats a string according to the given format specifier.
 *
 * @param buffer the buffer to write the formatted to
 * @param spec the format specifier
 * @return the number of bytes written
 */
size_t fmtlib_format(fmt_buffer_t *buffer, fmt_spec_t *spec);

#endif
