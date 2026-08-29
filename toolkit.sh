#!/bin/bash

# AHB7804R-V3 FW Modification Toolkit
# Non-destructive binary comparison and patching

set -e

usage() {
    echo "Usage: $0 [compare|patch]"
    echo ""
    echo "Commands:"
    echo "  compare <orig_bin> <live_bin> <patch_file.json> "
    echo "            - Analyzes differences and creates a patch file"
    echo ""
    echo "  patch   <stock_bin> <patch_file.json> <output_bin>"
    echo "            - Applies patch to stock image to create flashable binary"
    echo ""
    exit 1
}

if [ $# -lt 3 ]; then
    usage
fi

CMD=$1
shift

case "$CMD" in
    compare)
        node /tmp/AHB7804R-V3-tests/bin-diff.js "$1" "$2" "$3"
        ;;
    patch)
        node /tmp/AHB7804R-V3-tests/bin-patch.js "$1" "$2" "$3"
        ;;
    *)
        usage
        ;;
esac
