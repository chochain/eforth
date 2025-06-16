.( example 11.  calendars - print weekly calendars for any month in any year. )
decimal

variable t11
variable julian         ( 0 is 1/1/1950, good until 2050 )
variable leap           ( 1 for a leap year, 0 otherwise. )
( 1461 constant 4years  number of days in 4 years )

: year ( year --, compute julian date and leap year )
        dup
        1949 - 1461 4 */mod   ( days since 1/1/1949 )
        365 - julian !        ( 0 for 1/1/1950 )
        3 =                   ( modulus 3 for a leap year )
        if 1 else 0 then      ( leap year )
        leap !
        2000 =                ( 2000 is not a leap year )
        if 0 leap ! then
        ;

: first ( month -- 1st, 1st of a month from jan. 1 )
        dup 1 =
        if drop 0 
        else dup 2 =
            if drop 31 
            else dup 3 =
                if drop 59 leap @ + 
            else
                    4 - 30624 1000 */
                    90 + leap @ +       ( apr. 1 to dec. 1 )
            then   ( 59/60 for mar. 1 )
        then       ( 31 for feb. 1 )
    then           ( 0 for jan. 1 )
        ;

: stars 60 for 42 emit next ;    ( form the boarder )

: header ( -- )                  ( print title bar )
        cr stars cr 
        ."      sun     mon     tue     wed     thu     fri     sat"
        cr stars cr              ( print weekdays )
        ;

: blanks ( month -- )            ( skip days not in this month )
        first julian @ +         ( julian date of 1st of month )
        7 mod 8 * spaces ;       ( skip colums if not sunday   )

: days ( month -- )              ( print days in a month )
        dup first                ( days of 1st this month )
        swap 1 + first           ( days of 1st next month )
        over - 1 -               ( loop to print the days )
        1 swap                   ( first day count -- )
        for  2dup + 1 -
                julian @ + 7 mod ( which day in the week? )
                if else cr then  ( start a new line if sunday )
                dup  8 u.r       ( print day in 8 column field )
                1 +
        next
        2drop ;                  ( discard 1st day in this month )

: month ( n -- )                 ( print a month calendar )
        header dup blanks        ( print header )
        days cr stars cr ;       ( print days   )

: january       year 1 month ;
: february      year 2 month ;
: march         year 3 month ;
: april         year 4 month ;
: may           year 5 month ;
: june          year 6 month ;
: july          year 7 month ;
: august        year 8 month ;
: september     year 9 month ;
: october       year 10 month ;
: november      year 11 month ;
: december      year 12 month ;

.( to print the calender of may 2021, type: ) cr
.( 2024 may =>) cr
2024 may
