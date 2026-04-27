#!/bin/bash

make clean && make

#qemu-system-i386 -cdrom build/bach_k.iso -serial stdio
# qemu-system-i386 -m 16M -machine pc,accel=kvm -cdrom build/bach_k.iso -serial stdio

qemu-system-i386 -m 32M -machine pc,accel=kvm\
  -drive file=disk.img,format=raw,if=ide \
  -cdrom build/bach_k.iso \
  -d guest_errors \
  -serial stdio



