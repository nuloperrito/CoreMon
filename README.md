# CoreMon: Universal Windows CPU Monitor

CoreMon is a lightweight, high-performance monitoring application built for the Universal Windows Platform (UWP), compatible with both Windows 10/11 Desktop and Windows 10 Mobile. It provides detailed, real-time performance and clock speed metrics for every logical core of your CPU.

## Features

* **Per-Core Monitoring:** Displays the real-time current frequency (in MHz) and usage percentage for every logical processor core.
* **Detailed CPU Information:** Presents comprehensive CPU details, including the brand string, architecture (x64, ARM64, etc.), physical/logical core count, base clock speed, and supported instruction sets (like AVX2, NEON, AES, etc.).
* **Adjustable Refresh Rate:** The monitoring interval can be customized via a slider, ranging from 200 ms to 5000 ms (5 seconds).
* **Accurate Metrics:** Utilizes high-resolution, low-level Windows APIs (`NtQuerySystemInformation` and `CallNtPowerInformation`) for precise and timely data retrieval.

## Technology Stack

* **Platform:** Universal Windows Platform (UWP)
* **Language:** C++/CX (C++ with Component Extensions)
* **Target SDK:** Windows 10 (Version 10.0.22000.0 or later)
* **APIs:** Low-level Win32 system calls (`powrprof.h`, `winternl.h`) for hardware access.

## License

The repo is under MIT License. See [LICENSE](LICENSE.md) for more information.