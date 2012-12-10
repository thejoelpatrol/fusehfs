echo "Cleaning up autoconf leftovers..."
find . -name 'autom4te.cache' -exec rm -rf {} \; > /dev/null 2>&1
find . -name 'config.h.in' -exec rm -rf {} \; > /dev/null 2>&1
find . -name 'configure' -exec rm -rf {} \; > /dev/null 2>&1
find . -name '.stamp' -exec rm -rf {} \; > /dev/null 2>&1
