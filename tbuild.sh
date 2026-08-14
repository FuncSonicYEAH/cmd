#!/bin/sh
# Build script for generic UNIX systems.

set -e

echo "Notice: This script is for generic UNIX systems. For GNU/Linux, use the 'Makefile' instead."

if [ "y$CC" = "y" ]; then
    CC=cc
fi

if [ "y$V" = "y" ]; then
    V=1
fi

if [ "y$SYSV" = "y" ]; then
    SYSV=1
fi

if [ "$SYSV" = "1" ]; then
    SYSVFLAGS="-DLIBCMD_SYSV=1"
else
    SYSVFLAGS=""
fi

# All build output goes into the build/ directory.
mkdir -p build

# Compile for cmd.exe.
for src in *.c; do
    # shellcheck disable=SC2006
    # Support Bourne Shell.
    name=`basename "$src" .c`
    obj="build/$name.cmd.o"
    set -- "$CC" -c "$src" -o "$obj" -DLIBCMD_NO_VSNPRINTF=1 $SYSVFLAGS
    if [ "$V" = "1" ]; then
        echo "$*"
    fi
    "$@"
    if [ ! -f "$obj" ]; then
        # Intel System V C compiler don't support -o option. Use mv to rename the object file.
        mv "$name.o" "$obj"
    fi
done

# Link for cmd.exe.
set -- "$CC" build/*.cmd.o -o build/cmd.exe
if [ "$V" = "1" ]; then
    echo "$*"
fi
"$@" || (echo "Link failed, trying makefile or use SYSV=0 environment variable to build?" && exit 1)
if [ ! -f build/cmd.exe ]; then
    mv a.out build/cmd.exe
fi

# Compile for COMMAND.COM.
for src in *.c; do
    # shellcheck disable=SC2006
    # Support Bourne Shell.
    name=`basename "$src" .c`
    obj="build/$name.COMMAND.o"
    set -- "$CC" -c "$src" -o "$obj" -DLIBCMD_NO_VSNPRINTF=1 -DENABLE_AUTOEXEC=1 $SYSVFLAGS
    if [ "$V" = "1" ]; then
        echo "$*"
    fi
    "$@"
    if [ ! -f "$obj" ]; then
        # Intel System V C compiler don't support -o option. Use mv to rename the object file.
        mv "$name.o" "$obj"
    fi
done

# Link for COMMAND.COM.
set -- "$CC" build/*.COMMAND.o -o build/COMMAND.COM
if [ "$V" = "1" ]; then
    echo "$*"
fi
"$@" || (echo "Link failed, trying makefile or use SYSV=0 environment variable to build?" && exit 1)
if [ ! -f build/COMMAND.COM ]; then
    mv a.out build/COMMAND.COM
fi
