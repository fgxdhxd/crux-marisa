#!/bin/sh
# SPDX-License-Identifier: GPL-2.0

in="$1"
out="$2"

syscall_macro() {
    abi="$1"
    nr="$2"
    entry="$3"

    echo "__SYSCALL_${abi}($nr, $entry)"
}

emit() {
    abi="$1"
    nr="$2"
    entry="$3"
    compat="$4"
    umlentry=""

    if [ "$abi" = "64" -a -n "$compat" ]; then
	echo "a compat entry for a 64-bit syscall makes no sense" >&2
	exit 1
    fi

    # For CONFIG_UML, we need to strip the __x64_sys prefix
    if [ "${entry}" != "${entry#__x64_sys}" ]; then
	    umlentry="sys${entry#__x64_sys}"
    fi

    if [ -z "$compat" ]; then
	if [ -n "$entry" -a -z "$umlentry" ]; then
	    syscall_macro "$abi" "$nr" "$entry"
	elif [ -n "$umlentry" ]; then # implies -n "$entry"
	    echo "#ifdef CONFIG_X86"
	    syscall_macro "$abi" "$nr" "$entry"
	    echo "#else /* CONFIG_UML */"
	    syscall_macro "$abi" "$nr" "$umlentry"
	    echo "#endif"
	fi
    else
	echo "#ifdef CONFIG_X86_32"
	if [ -n "$entry" ]; then
	    syscall_macro "$abi" "$nr" "$entry"
	fi
	echo "#else"
	syscall_macro "$abi" "$nr" "$compat"
	echo "#endif"
    fi
}

grep '^[0-9]' "$in" | sort -n | (
    while read nr abi name entry compat; do
	abi=`echo "$abi" | tr '[a-z]' '[A-Z]'`
	emit "$abi" "$nr" "$entry" "$compat"
    done
) > "$out"
