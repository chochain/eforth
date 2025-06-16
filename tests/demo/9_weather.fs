.( example 9. weather reporting. )

variable t9
: weather ( nfarenheit -- )
        dup     55 <
        if      ." too cold!" drop
        else    85 <
                if      ." about right."
                else    ." too hot!"
                then
        then
        ;

.( you can type the following instructions: )
.( 90 weather =>) 90 weather cr
.( 70 weather =>) 70 weather cr
.( 32 weather =>) 32 weather cr cr
