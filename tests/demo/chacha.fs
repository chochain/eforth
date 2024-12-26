\
\ ChaCha20 parallel implementation
\
\ : th ( arr n -- &arr[n] ) cells swap + ;
: a@ ( n arr -- v ) swap th @ ;        \ fetch a[n]
: a! ( v n arr -- ) swap th ! ;        \ a[n] = v
: a+! ( v n arr -- ) swap th +! ;      \ a[n] += v
: 4@ ( a b c d arr -- va vb vc vd )    \ fetch array[a,b,c,d]
  >r ( keep arr )
  2swap i a@ swap i  a@ swap
  2swap i a@ swap r> a@ swap ;
: 4! ( a b c d va vb vc vd arr -- )    \ put array[a,b,c,d]
  >r ( keep arr )
  5 pick i a! 5 pick i  a!
  5 pick i a! 5 pick r> a!
  2drop 2drop ;
: rol ( v n -- v' )                    \ rotate left n-bits
  >r dup r@ lshift swap
  $20 r> - rshift or ;
\ quarter round
\   1.  a += b; d ^= a; d <<<= 16;  a b c d
\   2.  c += d; b ^= c; b <<<= 12;      c d a b
\   3.  a += b; d ^= a; d <<<= 8;           a b c d
\   4.  c += d; b ^= c; b <<<= 7;               c d a b
: hx ( c d a b n -- a' b c d' )       \ hash one line
  >r ( keep n  ) swap over + dup
  >r ( keep a' ) swap
  2swap  ( a' b c d )
  r> xor ( a' b c d^a' ) r> rol ;
: qround ( a b c d -- a b c d )
  2swap  ( c d a b )
  $10 hx ( a b c d ) $C hx ( c d a b )
  $8  hx ( a b c d ) $7 hx ( c d a b )
  2swap ;
  
\ test cases
create t0 $11111111 , $01020304 , $9b8d6f43 , $01234567 ,
\ > a = 0x11111111, b = 0x01020304, d = 0x01234567
\ > a = a + b = 0x11111111 + 0x01020304 = 0x12131415
\ > d = d ^ a = 0x01234567 ^ 0x12131415 = 0x13305172
\ > d = d<<<16 = 0x51721330
\ qround expected results
\ > abcd = $ea2a92f4 $cb1cf8ce $4581472e $5881c4bb  
\ 0 1 2 3 t0 4@ qround

\ ChaCha20 core
8 constant NT                        \ max number of threads
create hidx                          \ quater round indices
  $e943d872 , $cb61fa50 ,            \ diag 2, 3, 0, 1
  $fb73ea62 , $d951c840 ,            \ col  2, 3, 0, 1
create st                            \ st[16]
  $61707865 , $3320646e , $79622d32 , $6b206574 ,  \ signature
  $03020100 , $07060504 , $0b0a0908 , $0f0e0d0c ,  \ key1
  $13121110 , $17161514 , $1b1a1918 , $1f1e1d1c ,  \ key2
  $00000001 , $09000000 , $4a000000 , $00000000 ,  \ counter, nonce
create vbase NT $40 * allot          \ 64-byte tmp calc array
: vt ( -- ) vbase rank $40 * + ;     \ vbase offset by thread id
: st2vt ( -- )                       \ st := vt, or st vt $10 move
  $f for i st a@ i vt a! next ;      \ TODO: inc cntr
: vt+=st ( -- )                      \ vt += st
  $f for i st a@ i vt a+! next ;     \ TODO: xor data
: 4x8 ( dcba -- a b c d a b c d )    \ unpack (5% slower)
  3 for
    dup $f and swap 4 rshift
  next drop ;
: quarter ( bcda -- )                \ run a quarter round
  4x8 2over 2over vt 4@ qround vt 4! ;
: odd_even ( -- )                    \ column, and diag rounds
  3 for
    i hidx a@                        \ fetch indices
    dup        quarter               \ odd  quarter round
    $10 rshift quarter               \ even quarter round
  next ;
: one_block ( -- )
  st2vt
  $9 for odd_even next               \ 10x2 rounds
  vt+=st ;
  
\ validation
create gold                          \ expected vt after one_block
  $e4e7f110 , $15593bd1 , $1fdd0f50 , $c47120a3 ,
  $c7f4d1c7 , $0368c033 , $9aaa2204 , $4e6cd4c3 ,
  $466482d2 , $09aa9f07 , $05d7c214 , $a2028bd9 ,
  $d19c12b5 , $b94e16de , $e883d0cb , $4e3c50a2 ,
: check ( -- )                       \ validate against expected
  one_block
  $f for
    i vt a@ i gold a@
    <> if i . ." miss " cr then
  next
  ." done" rank . cr ;
: bench ( -- )
  clock negate
  9999 for one_block next            \ 640KB
  clock + lock . ." ms " cr unlock ;
\ multithreading
' bench constant xt                  \ keep thread starting point
: run ( n -- )
  clock negate >r
  1- dup >r for                      \ keep loop count
    xt task dup start                \ start threads, keep thread ids
  next
  r> for join next
  r> clock + . ." elapse ms" ;       \ waiting for threads to finish
4 run
bye

