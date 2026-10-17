\
\ Radix-2 FFT, Q15 fixed point, N=256, in-place decimation in time
\ (same style as chacha.fs: many short words, one stack comment each)
\
\ : th ( arr n -- &arr[n] ) cells swap + ;
: a@ ( n arr -- v ) swap th @ ;        \ fetch a[n]
: a! ( v n arr -- ) swap th ! ;        \ a[n] = v
$100 constant N                        \ points (256)
8    constant LOGN                     \ log2(N)
$80  constant TOL                      \ max |error| allowed vs exact DFT (LSB)
create re N cells allot                \ real part
create im N cells allot                \ imaginary part
\ twiddle w[t] = exp(-j*2*pi*t/N) * 2^15, t = 0..N/2-1
\ tables hold value + bias (all positive hex, same on every VM); fix removes it
create wr
  $10000 , $fff6 , $ffd9 , $ffa7 , $ff62 , $ff0a , $fe9d , $fe1e ,
  $fd8a , $fce4 , $fc2a , $fb5d , $fa7d , $f98a , $f885 , $f76c ,
  $f642 , $f505 , $f3b6 , $f255 , $f0e3 , $ef5f , $edca , $ec24 ,
  $ea6e , $e8a7 , $e6d0 , $e4e9 , $e2f2 , $e0ec , $ded7 , $dcb4 ,
  $da82 , $d843 , $d5f6 , $d39b , $d134 , $cec0 , $cc40 , $c9b4 ,
  $c71d , $c47b , $c1ce , $bf17 , $bc57 , $b98d , $b6ba , $b3df ,
  $b0fc , $ae11 , $ab1f , $a827 , $a528 , $a224 , $9f1a , $9c0c ,
  $98f9 , $95e2 , $92c8 , $8fab , $8c8c , $896b , $8648 , $8324 ,
  $8000 , $7cdc , $79b8 , $7695 , $7374 , $7055 , $6d38 , $6a1e ,
  $6707 , $63f4 , $60e6 , $5ddc , $5ad8 , $57d9 , $54e1 , $51ef ,
  $4f04 , $4c21 , $4946 , $4673 , $43a9 , $40e9 , $3e32 , $3b85 ,
  $38e3 , $364c , $33c0 , $3140 , $2ecc , $2c65 , $2a0a , $27bd ,
  $257e , $234c , $2129 , $1f14 , $1d0e , $1b17 , $1930 , $1759 ,
  $1592 , $13dc , $1236 , $10a1 , $0f1d , $0dab , $0c4a , $0afb ,
  $09be , $0894 , $077b , $0676 , $0583 , $04a3 , $03d6 , $031c ,
  $0276 , $01e2 , $0163 , $00f6 , $009e , $0059 , $0027 , $000a ,
create wi
  $8000 , $7cdc , $79b8 , $7695 , $7374 , $7055 , $6d38 , $6a1e ,
  $6707 , $63f4 , $60e6 , $5ddc , $5ad8 , $57d9 , $54e1 , $51ef ,
  $4f04 , $4c21 , $4946 , $4673 , $43a9 , $40e9 , $3e32 , $3b85 ,
  $38e3 , $364c , $33c0 , $3140 , $2ecc , $2c65 , $2a0a , $27bd ,
  $257e , $234c , $2129 , $1f14 , $1d0e , $1b17 , $1930 , $1759 ,
  $1592 , $13dc , $1236 , $10a1 , $0f1d , $0dab , $0c4a , $0afb ,
  $09be , $0894 , $077b , $0676 , $0583 , $04a3 , $03d6 , $031c ,
  $0276 , $01e2 , $0163 , $00f6 , $009e , $0059 , $0027 , $000a ,
  $0000 , $000a , $0027 , $0059 , $009e , $00f6 , $0163 , $01e2 ,
  $0276 , $031c , $03d6 , $04a3 , $0583 , $0676 , $077b , $0894 ,
  $09be , $0afb , $0c4a , $0dab , $0f1d , $10a1 , $1236 , $13dc ,
  $1592 , $1759 , $1930 , $1b17 , $1d0e , $1f14 , $2129 , $234c ,
  $257e , $27bd , $2a0a , $2c65 , $2ecc , $3140 , $33c0 , $364c ,
  $38e3 , $3b85 , $3e32 , $40e9 , $43a9 , $4673 , $4946 , $4c21 ,
  $4f04 , $51ef , $54e1 , $57d9 , $5ad8 , $5ddc , $60e6 , $63f4 ,
  $6707 , $6a1e , $6d38 , $7055 , $7374 , $7695 , $79b8 , $7cdc ,
\ gold: exact DFT of the test signal, bins 0..15
create gre
  $101f5b , $105a5e ,  $f9e07 , $e986b , $103178 ,  $e17cc , $11a3db ,  $fed36 ,
  $116f85 , $104a21 , $112190 , $f0921 ,  $faaf8 , $1047c9 , $10f7f7 , $106c82 ,
create gim
  $100000 , $1061c8 , $f6a5e ,  $e79c3 , $113d3a , $10c239 , $10d250 , $1046e4 ,
   $e6c0d ,  $fa455 , $f3722 , $10fbe4 , $10a879 ,  $e3f1d ,  $f853d ,  $eeec7 ,

\ complex multiply, Q15
: q15* ( a b -- a*b>>15 ) $8000 */ ;
: cmul ( xr xi wr wi -- tr ti )        \ (xr+j*xi)*(wr+j*wi)
  3 pick 2 pick q15*                   \ xr*wr
  3 pick 2 pick q15* -                 \ - xi*wi        = tr
  4 pick 2 pick q15*                   \ xr*wi
  4 pick 4 pick q15* +                 \ + xi*wr        = ti
  >r >r 2drop 2drop r> r> ;

\ butterfly: x[p] = x[p] + t, x[q] = x[p] - t   (one of re / im)
: put2 ( x t p q arr -- )
  >r
  3 pick 3 pick + 2 pick r@ a!         \ arr[p] = x + t
  3 pick 3 pick - over r> a!           \ arr[q] = x - t
  2drop 2drop ;
: twid ( t -- wr wi ) dup wr a@ swap wi a@ ;
: x2 ( q -- xr xi ) dup re a@ swap im a@ ;
: bre ( p q tr ti -- p q tr ti )       \ real half of the butterfly
  3 pick re a@ 2 pick 5 pick 5 pick re put2 ;
: bim ( p q tr ti -- )                 \ imaginary half
  3 pick im a@ over 5 pick 5 pick im put2 2drop 2drop ;
: bfly ( p q t -- )
  twid 2 pick x2 2swap cmul            \ p q tr ti = x[q] * w[t]
  bre bim ;

\ one group of h butterflies starting at g, twiddle stride s
: grp ( h g s -- h g s )
  2 pick 1- for
    over i + dup 4 pick + 2 pick i * bfly
  next ;
: stage ( h -- )                       \ all groups of half-size h
  dup 2* N swap /  dup 1- for          \ s = N/(2h) groups
    over 2* i * swap grp swap drop
  next 2drop ;

\ bit reversal permutation
: rev ( i -- r )
  0 swap LOGN 1- for
    dup 1 and rot 2* or swap 2/
  next drop ;
: xch ( i j arr -- )                   \ swap arr[i], arr[j]
  >r over r@ a@ over r@ a@ swap rot r@ a! swap r> a! ;
: brev1 ( i -- )
  dup rev 2dup < if 2dup re xch 2dup im xch then 2drop ;
: bitrev ( -- ) N 1- for i brev1 next ;

: fft ( -- )
  bitrev 1 LOGN 1- for dup stage 2* next drop ;

\ remove table bias once at load
: unb ( arr n bias -- )
  swap 1- for
    i 2 pick a@ over - i 3 pick a!
  next 2drop ;
: fix ( -- )
  wr $80 $8000 unb  wi $80 $8000 unb
  gre $10 $100000 unb  gim $10 $100000 unb ;
fix

\ test signal: 14-bit pseudo random, same on every VM (no division)
: lcg ( x -- x' ) 1+ $4b * 1- $10001 mod ;
: sig ( -- )
  1 N 1- for
    lcg dup $3fff and $2000 - i re a! 0 i im a!
  next drop ;

\ test: compare bins 0..15 with gold
: dre ( k -- d ) dup re a@ swap gre a@ - abs ;
: dim ( k -- d ) dup im a@ swap gim a@ - abs ;
: worst ( -- n ) 0 $f for i dre max i dim max next ;
: check ( -- ) sig fft worst . worst TOL <= . ;   \ prints max error, then -1 if ok
: bench ( -- ) 9999 for sig fft next ;           \ 10K transforms
bench
bye

