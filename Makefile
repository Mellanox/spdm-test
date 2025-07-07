# SPDX-License-Identifier: GPL-2.0-only or BSD-3-Clause
#
# Copyright (c) 2023 NVIDIA Corporation.
#

.PHONY: all kmod spdm_proxy spdm-emu patches spdm-prepare

all: spdm-emu spdm-proxy

CFLAGS = -Ilib -Wall
PSC_LIB = lib/libpsc_mailbox.a

kmod:
	cd kmod; make -C /lib/modules/$$(uname -r)/build M=$$PWD modules

spdm-proxy: spdm-proxy/spdm-proxy.c $(PSC_LIB)
	$(CC) $(CFLAGS) $^ -o spdm-proxy/$@

$(PSC_LIB) : lib/psc_mailbox.c
	$(CC) $(CFLAGS) -c lib/psc_mailbox.c -o lib/psc_mailbox.o
	$(AR) rcs $(PSC_LIB) lib/psc_mailbox.o

spdm-prepare:
	[ ! -f /usr/bin/aarch64-linux-gnu-gcc -a -f /usr/bin/aarch64-redhat-linux-gcc ] && \
	  ln -s /usr/bin/aarch64-redhat-linux-gcc /usr/bin/aarch64-linux-gnu-gcc || true
	[ ! -f /usr/bin/aarch64-linux-gnu-gcc-ar -a -f /usr/bin/gcc-ar ] && \
	  ln -s /usr/bin/gcc-ar /usr/bin/aarch64-linux-gnu-gcc-ar || true

spdm-emu: spdm-prepare
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
	cd spdm-emu; \
	  for i in ../patches/spdm-emu/*; do git am $$i; done
	cd spdm-emu/libspdm; \
	  for i in ../../patches/libspdm/*; do git am $$i; done

run:
	pkill spdm-proxy || true
	[ ! -e /dev/mem ] && insmod kmod/mlxbf-mmio.ko >&/dev/null || true
	./spdm-proxy/spdm-proxy &
	cd spdm-emu/build/bin; ./spdm_requester_emu --meas_op ALL
	pkill spdm-proxy || true

clean:
	$(RM) spdm-proxy/spdm-proxy lib/*.o lib/*.a *.o
	$(RM) -rf spdm-emu/build
