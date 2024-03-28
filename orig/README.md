\page 2 - Evolution of ceForth
Source codes kept under ~/orig/ting and details [here](https://chochain.github.io/eforth/orig/index.html)
<pre>
ceForth_10 - 2009       Dr. Ting first attempt of Forth in C
ceForth_23 - 2017-07-13 Dr. Ting last version of ceForth with pre-built ROM (compiled in F#)
ceForth_33 - 2019-07-01 Dr. Ting used CODE/LABEL/... functions as the macro assembler, 100% in C

ceForth_40 - 2021-07-27 Lee suggested Dr. Ting to use
                        + struct for dictionary entry with name and lambda pointers,
                        + std::vector for dict/ss/rs, and
                        + std::map to host dictionary
ceForth_40a- 2021-07-28 Lee suggested using VT macros to build dictionary entries (struct)
ceForth_40b- 2021-07-31 Lee replaced std::vector with ForthList struct for dict/ss/rs
ceForth_401- 2021-08-01 Dr. Ting adopted VT macro
ceForth_402- 2021-08-03 Dr. Ting adopted ForthList
ceForth_403- 2021-08-06 Lee refined _402
                        Dr. Ting add docs and presented it on Forth2020

ceForth_36 - 2021-09-27 Dr. Ting, learnt from _40x, upgraded his _33 to _36 (retained linear memory model)
ceForth_36a- 2021-10-03 Lee added CODE/IMMD macros
ceForth_36b- 2021-10-03 Dr. Ting added Code struct and lambda,
                        ported to esp32forth_85 and presented in Forth2020
ceForth_36x- 2022-01-13 Dr. Ting final archive, great for understanding Forth building

ceForth_410- 2024-03-25 Lee refactored _403 to pure vector-based i.e do away with SP, RP, WP, and IP.
</pre>
