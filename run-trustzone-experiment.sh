#!/bin/sh
if ! grep "^#define ATTACK_TZ$" disarmed.c; then
    echo "it looks like ATTACK_TZ is not defined in disarmed.c. please define it and try again."
    exit 1
fi

gcc disarmed.c -o disarmed

./disarmed

./pkcs_parse.py
