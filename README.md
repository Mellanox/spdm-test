                  BlueField-3 SPDM Test

This README describes how to run spdm-emu on BlueField-3 ARM core. It's
verified on Ubuntu 22.04.2 LTS (aarch64).

# Source Files
<pre>  
├── certs  
│   ├── ipn_root_cert.der        BlueField-3 IPN root certificate  
│   └── opn_root_cert.der        BlueField-3 OPN root certificate  
├── kmod                         Optional kernel module (needed for Linux kernel lockdown)  
│   ├── Kbuild  
│   └── mlxbf-mmio.c  
├── lib                          API for PSC mailbox  
│   ├── psc_mailbox.c  
│   └── psc_mailbox.h  
├── Makefile                     Makefile  
├── patches                      Patche files  
│   └── libspdm  
│       └── 0001-Fix-a-typo-in-libspdm_x509_compare_date_time.patch  
├── README.md  
├── spdm-emu                     spdm-emu submodule  
└── spdm-proxy                   SPDM proxy between spdm-emu and PSC  
    └── spdm-proxy.c  
</pre>
# Clone Source

> git clone git@github.com:Mellanox/spdm-test.git  
> cd spdm-test  
> git submodule update --init --recursive  

# Apply Patches

> make patches  

 Only one patch is available which has been posted to upstream libspdm
 repo. 

# Build

> make  

# Run

> make run  

 It'll start to run 'spdm-proxy' first, then 'spdm_requester_emu'.  
 
 Note: Load kmod/mlxbf-mmio.ko first if Linux kernel-lockdown is enabled.

 Expected output example:  
 ...  
 !!! verify_measurement_signature - PASS !!!

# CORIM/COMID Verification
> TBD
