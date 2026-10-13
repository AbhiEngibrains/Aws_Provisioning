[Click here](../README.md) to view the README.

## Design and implementation

This code example demonstrates how to perform AWS IoT Fleet Provisioning by Claim on PSOC&trade; Edge MCU devices. The complete flow demonstrated by this code example is described below.

  - Device connects to AWS IoT using a common claim certificate  
  - Device generates ECC P-256 key pair which is stored in PSA crypto persistent storage
  - Device creates a Certificate Signing Request (CSR), and publishes to AWS IoT Fleet Provisioning topic
  - AWS IoT returns a signed device certificate to the device
  - Device registers itself to AWS IOT (RegisterThing)
  - The received device certificate is stored in TF-M Protected Storage by the device
  - Device then reconnects using the new device certificate

 All PSOC&trade; Edge E84 MCU applications have a dual-CPU three-project structure to develop code for the CM33 and CM55 cores. The CM33 core has two separate projects for the secure processing environment (SPE) and non-secure processing environment (NSPE).

This code example demonstrates how to use Trusted Firmware-M (TF-M) with Infineon's PSOC&trade; Edge MCU. TF-M implements the SPE for Armv8-M and Armv8.1-M architectures (for example, the Cortex&reg;-M33, Cortex&reg;-M23, Cortex&reg;-M55, and Cortex&reg;-M85 processors) and dual core platforms. It is platform security architecture reference implementation aligning with PSA-certified guidelines, enabling chips, real-time operating systems, and devices to become PSA-certified. For more details, see the [Trusted Firmware-M documentation](https://tf-m-user-guide.trustedfirmware.org/). Logs from TF-M are configured to use SCB1, see the Device Configurator for moe details.
For more information on the TF-M library and services, see the [Getting started with Trusted Firmware-M (TF-M) on PSOC&trade; Edge](https://www.infineon.com/AN240096) application note.

The NSPE application ( proj_cm33_ns ) implements the application logic to establish the network connection to the AWS endpoint for device provisioning and subsequent MQTT connections. This code examples uses DEFINES+=CY_TFM_PSA_SUPPORTED  in the Makefile of *proj_cm33_ns* to ensure that the MbedTLS and Secure Sockets libraries will use PSA APIs for crypto operations. The PSA call will be serviced by TF-M which supports the PSA interface, and routes it to the MBedTLS library on the secure processing environment. The TF-M Makefile also contains appropriate config file references to be used by TF-M. See README.md file of the secure-sockets library for more details on how to configure this.

A project folder consists of various subfolders, each denoting a specific aspect of the project. The three project folders are as follows:

**Table 1. Application projects**

Project | Description
--------|------------------------
proj_cm33_s | TF-M (SPE)
proj_cm33_ns | M33 NSPE
proj_cm55 | M55 NSPE

<br>