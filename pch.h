//
// pch.h
// Header for standard system include files.
//

#pragma once

#include <collection.h>
#include <ppltasks.h>
#include <powrprof.h>
#include <Windows.h>
#include <winternl.h> // Added for NtQuerySystemInformation
#include "App.xaml.h"

// Definition for Processor Power Info
typedef struct _PROCESSOR_POWER_INFORMATION {
    ULONG  Number;
    ULONG  MaxMhz;
    ULONG  CurrentMhz;
    ULONG  MhzLimit;
    ULONG  MaxIdleState;
    ULONG  CurrentIdleState;
} PROCESSOR_POWER_INFORMATION, *PPROCESSOR_POWER_INFORMATION;

/*// Definition for Processor Cycle Time (Added for Frequency Calculation)
typedef struct _SYSTEM_PROCESSOR_CYCLE_TIME_INFORMATION {
    DWORD64 CycleTime;
} SYSTEM_PROCESSOR_CYCLE_TIME_INFORMATION, * PSYSTEM_PROCESSOR_CYCLE_TIME_INFORMATION;

// Definition for Processor Performance Info (Usage)
typedef struct _SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION {
    LARGE_INTEGER IdleTime;
    LARGE_INTEGER KernelTime;
    LARGE_INTEGER UserTime;
    LARGE_INTEGER DpcTime;
    LARGE_INTEGER InterruptTime;
    ULONG InterruptCount;
} SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION, *PSYSTEM_PROCESSOR_PERFORMANCE_INFORMATION;*/

// Define the info class ID for processor performance
#define SystemProcessorPerformanceInformation (SYSTEM_INFORMATION_CLASS)8
// Define the info class ID for processor cycle time
#define SystemProcessorCycleTimeInformation (SYSTEM_INFORMATION_CLASS)11
