# mcopy
Fast file copying using memory-mapping

This projects aims to develop a little program to copy files more efficiently.

The standard procedure to copy a file (e.g. using `cp`) is using a memory buffer to read the origin file into it and write it back to the target file (double-copy). If the file is big, it must perform many read-write operations or use a large amount of memory.

This program maps files into memory and treats them as strings and saves having to use read and write.

This procedure should be able to achieve **double speed** on copying files, while also saves memory.

As counterparts, not all kinds of devices allow to map files into memory, and I've found some problems using this program in devices with NTFS file-system.

Feel free to copy this project, test it and collaborate to improve it.
