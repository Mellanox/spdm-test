                  BlueField-3 SPDM Test

This README explains how to run spdm-emu from BlueField-3 ARM.

# Source files

.  
├── certs  
│   ├── ipn_root_cert.der        BlueField-3 IPN root certificate  
│   └── opn_root_cert.der        BlueField-3 OPN root certificate  
├── kmod                         Optional kernel module  
│   ├── Kbuild  
│   └── mlxbf-mmio.c  
├── lib                          API for PSC mailbox  
│   ├── psc_mailbox.c  
│   └── psc_mailbox.h  
├── Makefile                     Makefile  
├── patches                      patches  
│   └── libspdm  
│       └── 0001-Fix-a-typo-in-libspdm_x509_compare_date_time.patch  
├── README.md  
├── spdm-emu                     spdm-emu submodule  
└── spdm-proxy                   SPDM proxy between spdm-emu and PSC  
    └── spdm-proxy.c  

# Clone the source

> git clone git@github.com:Mellanox/spdm-test.git  
> cd spdm-test  
> git submodule update --init --recursive  

# Apply patches

> make patches  

 For now only one patch is available which has been posted to libspdm
 repo.

# Build

> make  

# Run

> make run  

 It'll start spdm-proxy first then run spdm_requester_emu.  
 
 Note: load kmod/mlxbf-mmio.ko first if Linux kernel-lockdown is enabled.

 Expected output example:  
 ...  
 !!! verify_measurement_signature - PASS !!!
