#!/bin/bash -x

insmod vpfs.ko
cat /proc/filesystems | grep vpfs

mkdir -p /mnt/vpfs
mount -t vpfs none /mnt/vpfs
cat /proc/mounts | grep vpfs

stat -f /mnt/vpfs

cd /mnt/vpfs
ls -latrhi

touch test_file.txt
ls -latrhi

echo "Text 1" >> test_file.txt
echo "Text 2" >> test_file.txt
cat test_file.txt

stat test_file.txt

cd -
umount /mnt/vpfs

rmmod vpfs
