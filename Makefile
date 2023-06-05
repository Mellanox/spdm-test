# SPDX-License-Identifier: GPL-2.0-only or BSD-3-Clause
#
# Copyright (c) 2023 NVIDIA Corporation.
#

.PHONY: all kmod spdm_proxy spdm-emu patches

all: kmod spdm-emu spdm-proxy

CFLAGS = -Ilib

kmod:
	cd kmod; make -C /lib/modules/$$(uname -r)/build M=$$PWD modules

spdm-proxy: spdm-proxy/spdm-proxy.c lib/libpsc_mailbox.a
	$(CC) $(CFLAGS) $^ -o spdm-proxy/$@

lib/libpsc_mailbox.a : lib/psc_mailbox.c
	$(CC) -c lib/psc_mailbox.c -o lib/psc_mailbox.o
	$(AR) rcs lib/libpsc_mailbox.a lib/psc_mailbox.o

spdm-emu:
	cd spdm-emu; mkdir build; cd build; \
	  cmake -DARCH=aarch64 -DTOOLCHAIN=AARCH64_GCC -DTARGET=Release -DCRYPTO=mbedtls ..; \
	  make copy_sample_key; \
	  make -j; \
	  cd ../../; \
	  if mlxbf-bootctl | grep 'GA Secured'; \
	    then cp certs/opn_root_cert.der spdm-emu/build/bin/ecp384/ca.cert.der; \
	    else cp certs/ipn_root_cert.der spdm-emu/build/bin/ecp384/ca.cert.der; \
	    fi

patches:
	cd spdm-emu/libspdm; \
	  for i in ../../patches/libspdm/*; do git am $$i; done

run:
	pkill spdm-proxy || true
	./spdm-proxy/spdm-proxy &
	cd spdm-emu/build/bin; ./spdm_requester_emu

clean:
	$(RM) spdm-proxy/spdm-proxy lib/*.o lib/*.a *.o
	cd kmod; make -C /lib/modules/$$(uname -r)/build M=$$PWD modules clean
	$(RM) -rf spdm-emu/build
