#!/bin/sh -e

if [[ -z $1 || ! -f $2 ]]; then
    echo "Usage: $0 outputPath binary.exe" >&2
    exit 1
fi

OUTPUT=$1
EXE=$2

mkdir -p $OUTPUT
rm -rf $OUTPUT/*

add_binary()
{
    destfile="$OUTPUT/`basename $1`"
    if [ ! -f $destfile ] ; then
        cp -anv $1 $destfile
        objdump -x $1 | grep "DLL Name" | cut -d: -f2 >${destfile}.objdeps
    fi
}

last_file_count=0
add_binary $EXE

while true ; do
    file_count=`ls -1 $OUTPUT/*.objdeps | wc -l`
    echo "count=$file_count"
    if [ $file_count -eq $last_file_count ] ; then break ; fi
    last_file_count=$file_count

    cat $OUTPUT/*.objdeps | sort | uniq | while read i ; do
        dll=`echo $i`  # fixup weird line endings
        if [[ -f /ucrt64/bin/$dll ]] ; then
            add_binary /ucrt64/bin/$dll
        fi
    done
done

rm -rf $OUTPUT/*.objdeps
