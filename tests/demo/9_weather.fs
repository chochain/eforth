.( example 9.  weather reporting. )

variable t9
: weather ( nfarenheit -- )
        cr
        dup     55 <
        if      ." too cold!" drop
        else    85 <
                if      ." about right."
                else    ." too hot!"
                then
        then
        ;

( you can type the following instructions )
\        90 weather
\        70 weather
\        32 weather
