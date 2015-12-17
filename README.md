# relocntfs
Adjusts the start of a NTFS partition

Original GPLv2 code:
- http://osdir.com/ml/linux.file-systems.ntfs.devel/2006-09/msg00073.html
- https://forums.hak5.org/index.php?/topic/18730-migrating-windows-between-disks/

Useful when an NTFS partition is cloned using ntfsclone to a different disk/partition.
Especially useful if the NTFS partition was a boot partition (with NTLDR), since NTLDR
relies on it to figure out here to boot from.

Do something like this to update the boot block:
```sh
./relocntfs -w /dev/sda1
```

## Changelog
- Added a Makefile and fixed some build issues.

