### fmt — a freestanding C string formatting library


```
Format Strings
==============

A format string is a string that contains zero or more format specifiers. A specifier
is a sequence of characters enclosed between '{' and '}'. To specify a literal '{' use
'{{' and to specify a literal '}', use '}' or '}}'.
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
        '-'       - right align (overrides align and zero)
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

         ** 'll' specifies a 64-bit argument, default is 32-bit **
        '[ll]d'         - signed decimal number
        '[ll]u'         - unsigned decimal number
        '[ll]b'         - unsigned binary number
        '[ll]o'         - unsigned octal number
        '[ll]x'         - unsigned hexadecimal number

        'f'             - floating point number (double)
        'F'             - floating point number capitalized

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

##### Benchmarks/Tests:
I would never claim this to be the fastest string formatting library, and performance
isn't a primary concern. However, the test suite also benchmarks the library to make
sure there isn't any obvious slowdowns.

Apple M1 Pro 10C 32GB
```
[PASS] "Hello, world!" in 151 ns
[PASS] "Hello, world!" in 129 ns
[PASS] "42" in 144 ns
[PASS] "2a" in 136 ns
[PASS] "3.14" in 151 ns
[PASS] "3.14, 42" in 182 ns
[PASS] "42, 3.14" in 187 ns
[PASS] "3.14, string, 42" in 214 ns
[PASS] "0x2a" in 142 ns
[PASS] "2A" in 138 ns
[PASS] "007" in 146 ns
[PASS] "-007" in 159 ns
[PASS] "+007" in 155 ns
[PASS] " 42" in 135 ns
[PASS] "-42" in 138 ns
[PASS] "3" in 171 ns
[PASS] "3.1" in 175 ns
[PASS] "  42" in 138 ns
[PASS] " 42 " in 157 ns
[PASS] "42  " in 150 ns
[PASS] "===== hello =====" in 160 ns
[PASS] "101............" in 175 ns
[PASS] "............101" in 166 ns
[PASS] "3.14" in 150 ns
[PASS] "3.141592653" in 171 ns
[PASS] "3.141592654" in 161 ns
[PASS] "3.141592654" in 161 ns
[PASS] "0.1234" in 138 ns
[PASS] "0.1234" in 146 ns
[PASS] "{42, 3}" in 268 ns
```
See `test/test.c` for the full test suite.
