### fmt — a freestanding C string formatting library


```
Format Strings
==============

A format string is a string that contains zero or more format specifiers.
The overall syntax of a format specifier is:

    {[index]:[[$fill]align][flags][width][.precision][type]}

index
    The index field is an optional positive integer that specifies the index of
    the argument to use for the value. Implicitly assigned indices begin at the
    first argument (0) and are incremented by 1 for each argument that is not
    explicitly assigned an index.

align
    The align field is an optional character that specifies the alignment of the output
    within the width of the field. A sequence of a '$' followed by a single non-null
    character may immediately precede the alignment marker to specify the character
    used for padding. If no alignment is specified, the output is left aligned using
    spaces. The following alignments are supported:

        '[$fill]<' - left justify
        '[$fill]^' - center justify
        '[$fill]>' - right justify

flags
    The flags field is a set of optional flags that modify the output.
    The following flags are supported:

        '#'       - alternate form
        '!'       - uppercase form
        '0'       - sets the fill character to '0'
                    for numeric values, pad with leading zeros up to width (conflicts with `align`)
        '+'       - always print sign for numeric values
        ' '       - leave a space in front of positive numeric values (conflicts with '+')

width
    The width field is an optional positive integer that specifies the minimum width
    of the output. After all other formatting is applied, the output is padded to the
    specified width using spaces or the fill character if specified in the align field.

    The width may also be specified using a '*' which will cause the next implicit argument
    to be used as the width, or as '*index' where index is a positive integer, which will
    use the specified argument as the width. When using the '*' syntax, the argument must
    be an integer.

precision
    The precision field is an optional positive integer.
    For floating point numbers, it specifies the number of digits to display after the
    decimal point. The default precision is 6 and the maximum precision is 9. The output
    is padded with trailing zeros if necessary.
    For integers, it specifies the minimum number of digits to display. By default, there
    is no minimum number of digits. The output is padded with leading zeros if necessary.
    For strings, it specifies the maximum number of characters to display. By default,
    strings are read until the first null character is found, but the precision field can
    be used to limit the number of characters read.

    The precision may be specified using a '*' or '*index' as described in the width field.

type
    The type field is an optional character or string that specifies the type of the
    argument. If no type is specified, the width and fill are respected, but no other
    formatting is applied.
    The following built-in types are supported:

        'd'             - signed decimal integer
        'u'             - unsigned decimal integer
        'b'             - unsigned binary integer
        'o'             - unsigned octal integer
        'x'             - unsigned hexadecimal integer
        'f'             - floating point number (double)

        's'             - string
        'c'             - character
        'p'             - pointer

Notes:
  - The maximum number of arguments supported by the fmt funcions is defined by the
    `FMT_MAX_ARGS` macro.
  - Implicit arguments are limited to `max_args` (default FMT_MAX_ARGS) and will ignore
    any specifiers which consume further arguments.  

Examples:
    {:d}      - integer
    {:05d}    - integer, sign-aware zero padding
    {:.2f}    - double, 2 decimal places
    {:>10u}   - unsigned, right justified with spaces
    {:$#^10d} - integer, center justified with '#'
    {:s}      - string
    {:.3s}    - string of specific length
    
```

Output of test/test.c:
```
[PASS] "Hello, world!" in 255 ns
[PASS] "Hello, world!" in 195 ns
[PASS] "42" in 210 ns
[PASS] "2a" in 210 ns
[PASS] "3.14" in 223 ns
[PASS] "3.14, 42" in 256 ns
[PASS] "42, 3.14" in 255 ns
[PASS] "3.14, string, 42" in 294 ns
[PASS] "0x2a" in 170 ns
[PASS] "2A" in 171 ns
[PASS] "007" in 186 ns
[PASS] "-007" in 192 ns
[PASS] "+007" in 183 ns
[PASS] " 42" in 160 ns
[PASS] "-42" in 154 ns
[PASS] "  42" in 153 ns
[PASS] " 42 " in 178 ns
[PASS] "42  " in 173 ns
[PASS] "===== hello =====" in 180 ns
```
