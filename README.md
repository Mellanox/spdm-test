# BlueField-3 SPDM Test

This README describes how to run spdm-emu on BlueField-3 ARM core. It's
verified on Ubuntu 22.04.2 LTS (aarch64).

## Source Files
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
## 1. Clone Source

Check date with the 'date' command, and fix it with command like 'date -s <Tue Jul  4 13:39:36 EDT 2023>' if needed.

> git clone git@github.com:Mellanox/spdm-test.git  
> (or git clone https://github.com/Mellanox/spdm-test.git)  
> cd spdm-test  
> git submodule update --init --recursive  

## 2. Apply Patches

> make patches  

## 3. Build

> \# Only needed once to install cmake.  
> apt install cmake  
> (or 'yum install cmake' on centos/redhat based distribution)  
> 
> make

## 4. Run

> make run  

 It'll start to run 'spdm-proxy' first, then 'spdm_requester_emu'.  
 
 Note: Need to sign and load kmod/mlxbf-mmio.ko first if Linux kernel-lockdown is enabled.

 Expected output example:  
 <pre>
 ...  
 write file - device_cert_chain_0.bin  
 ...  
 !!! verify_measurement_signature - PASS !!!  
 write file - device_measurement.bin  
 ...  
 </pre>

 device_cert_chain_0.bin & device_measurement.bin are located under
 spdm-emu/build/bin/

## CORIM/COMID Verification

### Convert SPDM measurement evidence to json file
> cd spdm-emu/build/bin  
> 
> python3 ../../spdm_emu/spdm_device_verifier_tool/SpdmMeasurement.py meas_to_json --meas device_measurement.bin --alg sha512 -o device_measurement.json

<pre>
# cat device_measurement.json | less
{
  "evidences": [
    {
      "evidence": {
        "index": 2,
        "digest": [
          8,
          "eddb3c9973a1c22ff35a5ec2e25b16968886b415104a3341118a4747a64615899b537737634b5fe2b236925d17227dd5c2287d1bf9eba427b3d43f8f744bd981"
        ]
      }
    },
    {
      "evidence": {
        "index": 3,
        "digest": [
          8,
          "9b98da5745784e93d2bff112007082c5de0869d4f86eebaf816ae1afef4fae2bbb393f2099c4cf781e6f6efa0ae2c773dbd3a17dfd9414672e27dae420bce590"
        ]
      }
    },
...
}
</pre>
