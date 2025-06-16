\
\ eForth v4.2 - autoexec demos 
\
18 constant TSZ                         \ 18 demos avaiable
create stb TSZ 2* cells allot           \ create an array to hold strings for demo files
variable si                             \ array index (default to 0)
: inc ( -- i ) si dup @ 1 rot +! ;      \ increment array idx
: s, ( adr len -- ) inc 2* >r           \ store a string (addr, len) into the table
  stb r@ 1+ th !                        \ string len
  stb r> th ! ;                         \ string addr
: s@ ( i -- ) 2* >r                     \ fetch a string (addr, len) from the table
  stb r@ th @
  stb r> 1+ th @ ;
: st_setup                              \ fill stable with strings
  s" tests/demo/1_hello.fs"       s,
  s" tests/demo/2_f.fs"           s,
  s" tests/demo/3_fig.fs"         s,
  s" tests/demo/4_loops.fs"       s,
  s" tests/demo/5_text.fs"        s,
  s" tests/demo/6_help.fs"        s,
  s" tests/demo/7_money.fs"       s,
  s" tests/demo/8_temperature.fs" s,
  s" tests/demo/9_weather.fs"     s,
  s" tests/demo/10_multiply.fs"   s,
  s" tests/demo/11_calendar.fs"   s,
  s" tests/demo/12_trig.fs"       s,
  s" tests/demo/13_sqrt.fs"       s,
  s" tests/demo/14_radix.fs"      s,
  s" tests/demo/15_ascii.fs"      s,
  s" tests/demo/16_random.fs"     s,
  s" tests/demo/17_array.fs"      s,
  s" tests/demo/18_does.fs"       s, ;
: st_lst
  ." ***list all eForth demos scripts***" cr
  TSZ 0 do
    i 1+ 3 .r ." : "
    i s@ type cr
  loop ;
: stars 60 for 61 emit next cr ;
: st_exe
  TSZ 0 do
    i s@ 2dup
    cr type stars included
    500 ms
  loop ;
st_setup
st_lst
st_exe
