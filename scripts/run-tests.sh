#!/bin/sh

set -e

cd "$(dirname "$0")/.."
make
cd target/

regexp=""
str=""

run_match() {
    name="$(mktemp finalXXX)"
    ./regex-to-c "$regexp" >> "$name".c

    cat << 'EOF' >> "$name".c
#include <stdio.h>

int main(int argc, char *argv[]) {
    int len = match(argv[1]);
    for (int i = 0; i < len; ++i)
        putchar(argv[1][i]);
    putchar('\n');
    return 0;
}
EOF

gcc "$name".c -o "$name"
./"$name" "$str"
rm "$name" "$name".c
}

success=0
failure=0

run() {
    regexp="$1"
    str="$2"
    expected="$3"
    result=$(run_match "$regexp" "$str")
    if [ "$result" = "$expected" ]; then
        success=$((success + 1))
    else
        failure=$((failure + 1))
        echo "FAIL: regex{$regexp} str{$str} expected{$expected} result{$result}"
    fi
}

# run {regex} {str} {expected}
run 'a' 'a' 'a'
run 'abcdefg' 'abcdefg' 'abcdefg'
run '[a-z]*' 'abcdefg' 'abcdefg'
run '(ab|cd)*' 'abcdcdabefg' 'abcdcdab'
run 'a*ax' 'ax' 'ax'
run 'a+|aab*' 'aab' 'aab'
run 'ab*c' 'ac' 'ac'
run 'x(ab)*y' '' ''

echo "$success SUCCESS"
echo "$failure FAILURE"
echo "$(( 100 * success / (success + failure) ))% passed"
