.( example 10.  print the multiplication table )

variable t10
: onerow ( nrow -- )
        cr
        dup 3 .r 4 spaces
        1 11
        for     2dup *
                4 .r
                1 +
        next
        2drop ;

: multiply ( -- )
        cr cr 7 spaces
        1 11
        for     dup 4 .r 1 +
        next drop 
        1 11
        for     dup onerow 1 +
        next drop cr
        ;
        
.( type 'multiply' to print the multiplication table =>) cr
multiply
