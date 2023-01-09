# Quark libmalloc

Quark Kernel is a hobbyist OS kernel mainly indended to be a challenging side-project.

## Building

You will need:
- GNU Autotools

To build the kernel for the x86 platform, run:
- `autoreconf -i`
- `./configure [--enable-tests] [--host=<host>] [--prefix=<your_desired_prefix>] [CFLAGS=-ffreestanding LDFLAGS=-nostdlib]`
- `make`
