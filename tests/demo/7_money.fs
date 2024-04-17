.( example 7.  money exchange )

variable t7
decimal
: nt      ( nnt -- $ )    100 3355 */  ;
: $nt     ( $ -- nnt )    3355 100 */  ;
: rmb     ( nrmb -- $ )   100 947 */  ;
: $rmb    ( $ -- njmp )   947 100 */  ;
: hk      ( nhk -- $ )    100 773 */  ;
: $hk     ( $ -- $ )      773 100 */  ;
: gold    ( nounce -- $ ) 285 *  ;
: $gold   ( $ -- nounce ) 285 /  ;
: silver  ( nounce -- $ ) 495 100 */  ;
: $silver ( $ -- nounce ) 100 495 */  ;
: ounce   ( n -- n, a word to improve syntax )  ;
: dollars ( n -- )      . ;

( with this set of money exchange words, we can do some tests: )
\        5 ounce gold . cr
\        10 ounce silver . cr
\        100 $nt . cr
\        20 $rmb . cr

( you can add many different currency bills then all up in dollars: )
\        1000 nt 500 hk + .s
\        320 rmb + .s
( print out total worth in dollars )
\        dollars cr

