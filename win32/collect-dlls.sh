#!/bin/sh -e

if [[ -z $1 || ! -f $2 ]]; then
    echo "Usage: $0 outputPath binary.exe" >&2
    exit 1
fi

OUTPUT=$1
EXE=$2

mkdir -p $OUTPUT
rm -rf $OUTPUT/*

cp -a $EXE $OUTPUT
last_file_count=0

while true ; do
    file_count=`ls -1 $OUTPUT/* | wc -l`
    echo "count=$file_count"
    if [ $file_count -eq $last_file_count ] ; then break ; fi
    last_file_count=$file_count

    objdump -x $OUTPUT/* | grep "DLL Name" | cut -d: -f2 | sort | uniq | while read dll ; do
        cp -anv /mingw32/bin/$dll $OUTPUT 2>/dev/null || true
    done
done
