//
// MainPage.xaml.cpp
// Implementation of the MainPage class.
//

#include "pch.h"
#include "MainPage.xaml.h"
#include <Windows.UI.ViewManagement.h>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <memory>
#include <locale>
#include <codecvt> 

#if defined(_M_IX86) || defined(_M_X64)
#include <intrin.h>
#include <bitset>
#endif

using namespace CoreMon;
using namespace Platform;
using namespace Platform::Collections;
using namespace Windows::Foundation;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::ViewManagement; // Namespace for ViewManagement

// Function pointer definition for NtQuerySystemInformation
typedef NTSTATUS(WINAPI* P_NtQuerySystemInformation)(
    SYSTEM_INFORMATION_CLASS SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength
    );

P_NtQuerySystemInformation pNtQuerySystemInformation = nullptr;

MainPage::MainPage()
{
    m_timer = ref new Windows::UI::Xaml::DispatcherTimer();
    m_refreshIntervalMs = 1000; // Default 1000ms (1 second)
    // Register the OnTick processor only once
    m_timer->Tick += ref new EventHandler<Object^>(this, &MainPage::OnTick);

    InitializeComponent();

    m_cpuDetails = GenerateCpuDetails();

    // Try to get the address of NtQuerySystemInformation from ntdll.dll
    HMODULE hNtDll = GetModuleHandle(L"ntdll.dll");
    if (hNtDll)
    {
        pNtQuerySystemInformation = (P_NtQuerySystemInformation)GetProcAddress(hNtDll, "NtQuerySystemInformation");
    }

    // Initialize Timer Frequency for high-res calculations
    LARGE_INTEGER freq;
    if (::QueryPerformanceFrequency(&freq))
    {
        m_tickFrequency = freq.QuadPart;
    }
    else
    {
        m_tickFrequency = 10000000; // Fallback
    }

    LARGE_INTEGER now;
    ::QueryPerformanceCounter(&now);
    m_prevTickTime = now.QuadPart;

    m_coresList = ref new Vector<CoreInfo^>();

    DWORD coreCount = GetCpuCoreCount();

    // Resize our previous time storages
    m_prevCoreTimes.resize(coreCount);
    memset(m_prevCoreTimes.data(), 0, coreCount * sizeof(CoreTimeData));

    m_prevCycleTimes.resize(coreCount);
    std::fill(m_prevCycleTimes.begin(), m_prevCycleTimes.end(), 0);

    // -------------------------------------

    // 1. Initial Frequency Fetch (to get MaxMhz)
    int sizePower = coreCount * sizeof(PROCESSOR_POWER_INFORMATION);
    LPBYTE pBufferPower = new BYTE[sizePower];

    if (::CallNtPowerInformation(ProcessorInformation, NULL, 0, pBufferPower, sizePower) == 0)
    {
        PPROCESSOR_POWER_INFORMATION ppi = (PPROCESSOR_POWER_INFORMATION)pBufferPower;
        for (DWORD i = 0; i < coreCount; i++)
        {
            String^ name = "Core " + i.ToString();
            int maxMhz = ppi[i].MaxMhz;
            auto coreInfo = ref new CoreInfo(name, maxMhz);
            m_coresList->Append(coreInfo);
        }
    }
    delete[] pBufferPower;

    // 2. Initial Usage and Cycle Fetch
    if (pNtQuerySystemInformation)
    {
        // Init Usage Baseline
        int sizePerf = coreCount * sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION);
        LPBYTE pBufferPerf = new BYTE[sizePerf];
        ULONG returnLength = 0;

        NTSTATUS status = pNtQuerySystemInformation(
            SystemProcessorPerformanceInformation,
            pBufferPerf,
            sizePerf,
            &returnLength);

        if (status == 0)
        {
            PSYSTEM_PROCESSOR_PERFORMANCE_INFORMATION pspi = (PSYSTEM_PROCESSOR_PERFORMANCE_INFORMATION)pBufferPerf;
            for (DWORD i = 0; i < coreCount; i++)
            {
                m_prevCoreTimes[i].IdleTime = pspi[i].IdleTime.QuadPart;
                m_prevCoreTimes[i].KernelTime = pspi[i].KernelTime.QuadPart;
                m_prevCoreTimes[i].UserTime = pspi[i].UserTime.QuadPart;
            }
        }
        delete[] pBufferPerf;

        // Init Cycle Baseline (for Frequency)
        int sizeCycles = coreCount * sizeof(SYSTEM_PROCESSOR_CYCLE_TIME_INFORMATION);
        LPBYTE pBufferCycles = new BYTE[sizeCycles];

        status = pNtQuerySystemInformation(
            SystemProcessorCycleTimeInformation,
            pBufferCycles,
            sizeCycles,
            &returnLength);

        if (status == 0)
        {
            PSYSTEM_PROCESSOR_CYCLE_TIME_INFORMATION psci = (PSYSTEM_PROCESSOR_CYCLE_TIME_INFORMATION)pBufferCycles;
            for (DWORD i = 0; i < coreCount; i++)
            {
                m_prevCycleTimes[i] = psci[i].CycleTime;
            }
        }
        delete[] pBufferCycles;
    }

    StartTimerAndRegisterHandler();
}

void MainPage::StartTimerAndRegisterHandler()
{
    // Refactored to use the member timer m_timer and m_refreshIntervalMs.
    if (m_timer->IsEnabled)
    {
        m_timer->Stop();
    }

    // Set Interval from the member variable (converted to 100-nanosecond units)
    TimeSpan ts;
    // 1ms = 10,000 units
    ts.Duration = (long long)m_refreshIntervalMs * 10000;

    m_timer->Interval = ts;
    m_timer->Start();
}

void MainPage::OnRefreshSliderValueChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs^ e)
{
    // The NewValue is a double, cast it to int since the step is 100ms.
    m_refreshIntervalMs = (int)e->NewValue;

    // Update the TextBlock that displays the value.
    if (RefreshRateText)
    {
        std::wstringstream ss;
        double seconds = (double)m_refreshIntervalMs / 1000.0;

        // Format the output: "1000 ms (1.0 s)"
        ss << m_refreshIntervalMs << L" ms (";
        // Use one decimal place for seconds
        ss << std::fixed << std::setprecision(1) << seconds << L" s)";

        RefreshRateText->Text = ref new Platform::String(ss.str().c_str());
    }

    // Restart the timer with the new interval
    StartTimerAndRegisterHandler();
}

void MainPage::OnTick(Object^ sender, Object^ e)
{
    UpdateCoreData();
}

DWORD MainPage::GetCpuCoreCount()
{
    SYSTEM_INFO si = { 0 };
    ::GetSystemInfo(&si);
    return si.dwNumberOfProcessors;
}

void MainPage::UpdateCoreData()
{
    DWORD coreCount = (DWORD)m_coresList->Size;
    if (coreCount == 0 || !pNtQuerySystemInformation) return;

    // Get current high-res time
    LARGE_INTEGER now;
    ::QueryPerformanceCounter(&now);
    int64_t currentTickTime = now.QuadPart;
    double timeDeltaSeconds = (double)(currentTickTime - m_prevTickTime) / (double)m_tickFrequency;

    // Prevent division by zero if tick is too fast
    if (timeDeltaSeconds <= 0.0) timeDeltaSeconds = 0.0001;

    // --- Part 1: Update Frequency (Using Cycle Time) ---
    // Instead of using cached ProcessorInformation, we calculate Effective Frequency from Cycle Delta.
    int sizeCycles = coreCount * sizeof(SYSTEM_PROCESSOR_CYCLE_TIME_INFORMATION);
    LPBYTE pBufferCycles = new BYTE[sizeCycles];
    ULONG returnLength = 0;

    NTSTATUS statusCycles = pNtQuerySystemInformation(
        SystemProcessorCycleTimeInformation,
        pBufferCycles,
        sizeCycles,
        &returnLength);

    if (statusCycles == 0)
    {
        PSYSTEM_PROCESSOR_CYCLE_TIME_INFORMATION psci = (PSYSTEM_PROCESSOR_CYCLE_TIME_INFORMATION)pBufferCycles;
        for (DWORD i = 0; i < coreCount; i++)
        {
            CoreInfo^ core = m_coresList->GetAt(i);

            uint64_t currentCycles = psci[i].CycleTime;
            uint64_t prevCycles = m_prevCycleTimes[i];

            // Handle overflow (rare but possible)
            uint64_t deltaCycles = (currentCycles >= prevCycles) ? (currentCycles - prevCycles) : 0;

            // Calculate Hz = Cycles / Seconds
            double hz = (double)deltaCycles / timeDeltaSeconds;

            // Convert to MHz
            int mhz = (int)(hz / 1000000.0);

            // Fallback: If cycle reading fails or yields 0 (e.g., deep sleep), maybe show 0 or keep last?
            // Usually 0 means idle in some accounting, or just low power. 
            // We set it.
            core->CurrentFrequency = mhz;

            // Update baseline
            m_prevCycleTimes[i] = currentCycles;
        }
    }
    else
    {
        int sizePower = coreCount * sizeof(PROCESSOR_POWER_INFORMATION);
        LPBYTE pBufferPower = new BYTE[sizePower];

        if (::CallNtPowerInformation(ProcessorInformation, NULL, 0, pBufferPower, sizePower) == 0)
        {
            PPROCESSOR_POWER_INFORMATION ppi = (PPROCESSOR_POWER_INFORMATION)pBufferPower;
            for (DWORD i = 0; i < coreCount; i++)
            {
                CoreInfo^ core = m_coresList->GetAt(i);
                // Note: On some systems, CurrentMhz might not reflect instantaneous Turbo speeds
                // but it is the best available API in UWP.
                core->CurrentFrequency = ppi[i].CurrentMhz;
            }
        }
        delete[] pBufferPower;
    }
    delete[] pBufferCycles;

    // --- Part 2: Update Usage ---
    int sizePerf = coreCount * sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION);
    LPBYTE pBufferPerf = new BYTE[sizePerf];

    NTSTATUS statusPerf = pNtQuerySystemInformation(
        SystemProcessorPerformanceInformation,
        pBufferPerf,
        sizePerf,
        &returnLength);

    if (statusPerf == 0)
    {
        PSYSTEM_PROCESSOR_PERFORMANCE_INFORMATION pspi = (PSYSTEM_PROCESSOR_PERFORMANCE_INFORMATION)pBufferPerf;

        for (DWORD i = 0; i < coreCount; i++)
        {
            CoreInfo^ core = m_coresList->GetAt(i);

            int64_t currentIdle = pspi[i].IdleTime.QuadPart;
            int64_t currentKernel = pspi[i].KernelTime.QuadPart;
            int64_t currentUser = pspi[i].UserTime.QuadPart;

            int64_t deltaIdle = currentIdle - m_prevCoreTimes[i].IdleTime;
            int64_t deltaKernel = currentKernel - m_prevCoreTimes[i].KernelTime;
            int64_t deltaUser = currentUser - m_prevCoreTimes[i].UserTime;

            int64_t totalSystem = deltaKernel + deltaUser;
            int64_t systemBusy = totalSystem - deltaIdle;

            int usagePercent = 0;
            if (totalSystem > 0)
            {
                usagePercent = (int)((systemBusy * 100) / totalSystem);
            }

            if (usagePercent < 0) usagePercent = 0;
            if (usagePercent > 100) usagePercent = 100;

            core->UsagePercent = usagePercent;

            m_prevCoreTimes[i].IdleTime = currentIdle;
            m_prevCoreTimes[i].KernelTime = currentKernel;
            m_prevCoreTimes[i].UserTime = currentUser;
        }
    }
    delete[] pBufferPerf;

    // Update timestamp for next tick
    m_prevTickTime = currentTickTime;
}

Platform::String^ GetCpuBrandStringFromRegistry()
{
    HKEY hKey;
    WCHAR buffer[256] = {};
    DWORD bufferSize = sizeof(buffer);
    Platform::String^ cpuName = nullptr;

    long status = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey);

    if (status == ERROR_SUCCESS)
    {
        status = RegQueryValueExW(hKey, L"ProcessorNameString", nullptr, nullptr, (LPBYTE)buffer, &bufferSize);
        if (status == ERROR_SUCCESS)
        {
            cpuName = ref new Platform::String(buffer);
        }
        RegCloseKey(hKey);
    }

    return cpuName;
}

Platform::String^ MainPage::GenerateCpuDetails()
{
    std::wstringstream ss;

    // 1. Architecture and Basics
    SYSTEM_INFO si;
    ::GetNativeSystemInfo(&si);

    std::wstring arch;
    switch (si.wProcessorArchitecture) {
    case PROCESSOR_ARCHITECTURE_AMD64: arch = L"x64 (AMD64)"; break;
    case PROCESSOR_ARCHITECTURE_ARM: arch = L"ARM"; break;
    case PROCESSOR_ARCHITECTURE_ARM64: arch = L"ARM64"; break;
    case PROCESSOR_ARCHITECTURE_INTEL: arch = L"x86"; break;
    default: arch = L"Unknown"; break;
    }

    // 2. CPU Brand String (Name)
    Platform::String^ registryCpuName = GetCpuBrandStringFromRegistry();
    std::wstring brandStringW;

    if (registryCpuName != nullptr)
    {
        brandStringW = registryCpuName->Data();
    }
    else
    {
#if defined(_M_IX86) || defined(_M_X64)
        int cpuInfo[4] = { -1 };
        char brand[0x40];
        memset(brand, 0, sizeof(brand));

        // Check if extended functions are supported
        __cpuid(cpuInfo, 0x80000000);
        if (cpuInfo[0] >= 0x80000004) {
            __cpuid((int*)(brand), 0x80000002);
            __cpuid((int*)(brand + 16), 0x80000003);
            __cpuid((int*)(brand + 32), 0x80000004);
            std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
            std::wstring brandStringW = converter.from_bytes(brand);
        }
#else
        // Fallback for ARM where __cpuid is not available in the same way
        if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64)
            brandStringW = L"ARM64 Processor";
        else if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM)
            brandStringW = L"ARM Processor";
        else
            brandStringW = L"Unknown Processor";
#endif
    }

    // Remove leading/trailing spaces from brand string
    size_t first = brandStringW.find_first_not_of(' ');
    if (std::string::npos != first) {
        size_t last = brandStringW.find_last_not_of(' ');
        brandStringW = brandStringW.substr(first, (last - first + 1));
    }

    // 3. Physical vs Logical Cores
    DWORD logicalCount = 0;
    DWORD physicalCount = 0;
    DWORD len = 0;
    
    // First call to determine buffer size
    ::GetLogicalProcessorInformation(NULL, &len);
    if (len > 0) {
        std::vector<uint8_t> buffer(len);
        PSYSTEM_LOGICAL_PROCESSOR_INFORMATION ptr = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)buffer.data();
        if (::GetLogicalProcessorInformation(ptr, &len)) {
            DWORD count = len / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
            for (DWORD i = 0; i < count; i++) {
                if (ptr[i].Relationship == RelationProcessorCore) {
                    physicalCount++;
                    // Count bits set in ProcessorMask for logical count per core (or use accumulator)
                    // But simpler to just trust GetSystemInfo for total logical
                }
            }
        }
    }
    logicalCount = si.dwNumberOfProcessors; // Total logical from SystemInfo

    // 4. Base Frequency (Default Clock) - Approx from Power Info
    // (Note: This might be Max Turbo on some systems, or Base on others, but it's the "Max" capacity reported)
    int baseMhz = 0;
    int sizePower = logicalCount * sizeof(PROCESSOR_POWER_INFORMATION);
    LPBYTE pBufferPower = new BYTE[sizePower];
    if (::CallNtPowerInformation(ProcessorInformation, NULL, 0, pBufferPower, sizePower) == 0) {
        PPROCESSOR_POWER_INFORMATION ppi = (PPROCESSOR_POWER_INFORMATION)pBufferPower;
        baseMhz = ppi[0].MaxMhz; // Assume all cores have same base
    }
    delete[] pBufferPower;


    // 5. Instruction Sets
    std::vector<std::wstring> features;
#if defined(_M_IX86) || defined(_M_X64)
    int info[4];
    __cpuid(info, 1); // Standard features
    if (info[2] & (1 << 0)) features.push_back(L"SSE3");
    if (info[2] & (1 << 9)) features.push_back(L"SSSE3");
    if (info[2] & (1 << 19)) features.push_back(L"SSE4.1");
    if (info[2] & (1 << 20)) features.push_back(L"SSE4.2");
    if (info[2] & (1 << 25)) features.push_back(L"AES");
    if (info[2] & (1 << 28)) features.push_back(L"AVX");
    if (info[2] & (1 << 12)) features.push_back(L"FMA3");

    __cpuid(info, 7); // Extended features
    if (info[1] & (1 << 5)) features.push_back(L"AVX2");
    if (info[1] & (1 << 29)) features.push_back(L"SHA");

#elif defined(_M_ARM) || defined(_M_ARM64)
    // Check for common ARM features via IsProcessorFeaturePresent

    // Basic SIMD & Float
    if (IsProcessorFeaturePresent(PF_ARM_NEON_INSTRUCTIONS_AVAILABLE)) features.push_back(L"NEON (ASIMD)");
    if (IsProcessorFeaturePresent(PF_ARM_VFP_32_REGISTERS_AVAILABLE)) features.push_back(L"VFPv3/v4");

    // ARMv8 Crypto Extensions
    // Note: The Windows API often binds AES and SHA to a single flag, but may also use separate flags.
    if (IsProcessorFeaturePresent(PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE)) features.push_back(L"AES/SHA1/SHA2");
    
    // CRC32 (usually optional for ARMv8.0, required for ARMv8.1)
    if (IsProcessorFeaturePresent(PF_ARM_V8_CRC32_INSTRUCTIONS_AVAILABLE)) features.push_back(L"CRC32");
    
    // LSE Atomics (Large System Extensions - ARMv8.1)
    if (IsProcessorFeaturePresent(PF_ARM_V81_ATOMIC_INSTRUCTIONS_AVAILABLE)) features.push_back(L"LSE Atomics");
    
    // Dot Product (For Machine Learning - ARMv8.2/v8.4)
    if (IsProcessorFeaturePresent(PF_ARM_V82_DP_INSTRUCTIONS_AVAILABLE)) features.push_back(L"DotProduct");
    
    // JavaScript Converter (ARMv8.3)
    if (IsProcessorFeaturePresent(PF_ARM_V83_JSCVT_INSTRUCTIONS_AVAILABLE)) features.push_back(L"JSCVT");
    
    // Etc.
    if (IsProcessorFeaturePresent(PF_ARM_DIVIDE_INSTRUCTION_AVAILABLE)) features.push_back(L"HW Divide");
    if (IsProcessorFeaturePresent(PF_ARM_FMAC_INSTRUCTIONS_AVAILABLE)) features.push_back(L"FMAC");
#endif

    // Building the output string
    ss << L"CPU: " << std::wstring(brandStringW.begin(), brandStringW.end()) << L"\n";
    ss << L"Architecture: " << arch << L"\n";
    ss << L"Cores: " << physicalCount << L" Physical, " << logicalCount << L" Logical\n";
    ss << L"Base Clock: " << baseMhz << L" MHz\n";
    
    ss << L"Instructions: ";
    for (size_t i = 0; i < features.size(); ++i) {
        ss << features[i];
        if (i < features.size() - 1) ss << L", ";
    }
    if (features.empty()) ss << L"Standard Set";

    return ref new Platform::String(ss.str().c_str());
}
