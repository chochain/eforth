cr .( example 15. ascii character table )

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

cr .( type 'table' to display ASCII char table )
table cr


