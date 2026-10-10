set pagination off
set confirm off
set architecture riscv:rv64
file bin/kernel
target remote localhost:12345
echo === RESET ===

info registers pc
x/6i $pc
x/2gx 0x1018
si
info registers pc t0
si
info registers pc a2
si
info registers pc a0
si
info registers pc a1
si
info registers pc t0
si
echo === FIRMWARE ENTRY ===

info registers pc a0 a1 a2
x/8i $pc
x/6gx 0x1028
break *0x80200000
continue
echo === KERNEL ENTRY ===

info registers pc sp ra a0 a1
info registers priv
p/x &bootstack
p/x &bootstacktop
x/3i $pc
si
info registers pc sp
si
info registers pc sp
si
echo === C ENTRY ===

info registers pc sp ra
detach
quit
