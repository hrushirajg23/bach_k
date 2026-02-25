Since the 32bit setup is already done, we just need to set path

1. export PATH="$HOME/opt/cross/bin:$PATH"

2. make

3. For display based qemu-system-i386 -cdrom build/yegaos.iso

    For serial based 

qemu-system-i386 -cdrom build/yegaos.iso -serial stdio

**** running via hard-disk ***

qemu-system-i386 \
  -drive file=disk.img,format=raw,if=ide \
  -cdrom build/yegaos.iso \
  -d guest_errors \
  -serial stdio

For mount the file system

sudo mount -t ext2 -o loop disk.img /mnt
