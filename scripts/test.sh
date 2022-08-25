#!/bin/bash

if [[ $# -ne 2 ]]; then
  echo "usage: test.sh {regexp} {str}"
  exit 1
fi

regexp=$1
str=$2

(
  cd "$(dirname "$0")" || exit 1

  cd ../target/ || exit 1

  name="$(mktemp finalXXX)"

  cat << EOF > "$name".c
#include <stdio.h>
#include <string.h>

EOF

  ./regex-to-c "$regexp" >> "$name".c

  cat << EOF >> "$name".c
int main(int argc, char *argv[]) {
    char reg[128];
    int len, success;
    strcpy(reg, argv[1]);
    if (match(&len, &success, reg)) {
        printf("match for %d chars\n", len);
    } else {
        printf("not match\n");
    }
    return 0;
}
EOF

  gcc "$name".c -o "$name"
  ./"$name" "$str"
  rm "$name" "$name".c
)

