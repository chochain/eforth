.( example 14.      radix for number conversions )
variable t14_15

decimal
: octal  8 base !  ;
: binary 2 base !  ;

( try converting numbers among different radices: )
\        decimal 12345 hex  .
\        hex abcd decimal  .
\        decimal 100 binary  .
\        binary 101010101010 decimal  .

.( example 15.      ascii character table )

variable t15
: character ( n -- )
        dup emit hex dup 3 .r
        octal dup 4 .r
        decimal 4 .r
        2 spaces
        ;

: line ( n -- )
        cr
        5 for   dup character
                16 +
        next
        drop ;

: table ( -- )
        32
        15 for  dup line
                1 +
        next
        drop ;

( type 'table' to display ASCII char table )


