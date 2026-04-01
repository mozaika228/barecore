set pagination off
set disassemble-next-line on
target remote :1234
symbol-file build/kernel.elf
break kmain
continue
