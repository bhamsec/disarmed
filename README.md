# DisARMed: Attacking ARM TrustZone from Userspace with Memory Aliasing

This repository contains our implementation of the DisARMed attack, a mitigation for DisARMed, and the tooling we wrote to help develop it.

## Structure

- `disarmed.c`: The DisARMed attack implementation. It contains both an implementation of the Linux kernel attack and the TrustZone attack for the NXP LS1046ARDB platform. Additionally contained is root-only debugging code to assist in development, which is excluded by default. Tested with GCC 15, compile with `gcc disarmed.c -o disarmed`.
- `mitigation.patch`: A patch to ARM TF-A 2.10.0. It demonstrates a proof-of-concept alias detection mechanism that can be implemented as a mitigation. When memory aliasing is detected, the firmware panics to prevent aliasing from being used later.
- `uefi-scan`: A UEFI executable that we wrote to help determine the outcomes of SPD modification on ARMv8-A platforms. It performs a single sweep through a region to detect aliasing. Tested with clang 20, use GNU make to build. Also requires autotools.
- `qemu.patch`: A patch to QEMU 9.2.3 that will emulate the same memory aliasing behaviour that we have observed on our test platforms.
- `gdb.py`: A GDB script for examining page tables in memory. Requires physical addressing, which can be enabled in QEMU with `maintenance packet Qqemu.PhyMemMode:1`.
- `optee_ta`: A simple OP-TEE trusted applet that was used for testing.
- `pkcs_parse.py`: Extracts 4096-bit RSA private keys stored by the OP-TEE PKCS#11 trusted applet from a dump of secure memory. We used this in our evaluation to recover private key material from dumps of secure memory performed via DisARMed.

## Tested Platforms
We tested DisARMed on the ARM Morello SDP, Ampere Altra DP, and NXP LS1046ARDB. Each platform has 16GB of memory installed, modified to appear as 32GB. They had 1G hugepages reserved. The ARM Morello SDP and NXP LS1046ARDB were running the OP-TEE Trusted Execution Environment.
