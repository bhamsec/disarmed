#!/usr/bin/env python3
# pycryptodome
from Crypto.PublicKey import RSA

# this retrieves a 4096-bit rsa private key. to add one to the tee:
# alias p11="pkcs11-tool --module /usr/lib/libckteec.so"
# p11 --init-token --label testtoken --so-pin 12341234
# p11 --label testtoken --login --so-pin 12341234 --init-pin --pin 12341234

# then, for a existing key:
# p11 --write-object priv.der --type privkey --id 1234 --login --pin 12341234 --usage-sign --usage-derive --label testkey

# or, to generate a key:
# p11 --login --pin 12341234 --keypairgen --label testkey --key-type rsa:4096

FILE = "memory.dmp"
dmp = b""

with open(FILE, "rb") as f:
    dmp = f.read()

j = dmp.find(b"\x22\x01")
while j != -1:
    try:
        i = j
        modulus = int.from_bytes(dmp[i - 513:i], byteorder="big")
        i = dmp.find(b"\x23\x01", i)
        public_exponent = int.from_bytes(dmp[i - 3: i], byteorder="big")
        i = dmp.find(b"\x24\x01", i)
        private_exponent = int.from_bytes(dmp[i - 512:i], byteorder="big")
        i = dmp.find(b"\x25\x01", i)
        prime1 = int.from_bytes(dmp[i - 257:i], byteorder="big")
        i = dmp.find(b"\x26\x01", i)
        prime2 = int.from_bytes(dmp[i - 257:i], byteorder="big")

        private_key = RSA.construct((modulus, public_exponent, private_exponent, prime1, prime2))

        print(private_key.export_key(format="PEM").decode("utf-8"))
    except:
        pass
    j = dmp.find(b"\x22\x01", j + 1)
