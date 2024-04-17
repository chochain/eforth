.( example 8. temperature conversion )

variable t8
: f>c ( nfarenheit -- ncelcius )
        32 -
        10 18 */
        ;

: c>f ( ncelcius -- nfarenheit )
        18 10 */
        32 +
        ;

( try these commands )
\ 90 f>c . cr
\ 0  c>f . cr

