//
// MainPage.xaml.h
// Declaration of the MainPage class.
//

#pragma once

#include "MainPage.g.h"
#include <vector>

namespace CoreMon
{
    // Helper struct to store previous CPU times for calculation
    struct CoreTimeData {
        int64_t IdleTime;
        int64_t KernelTime;
        int64_t UserTime;
    };

    // Define a class that implements INotifyPropertyChanged for data binding
    [Windows::UI::Xaml::Data::Bindable]
    public ref class CoreInfo sealed : Windows::UI::Xaml::Data::INotifyPropertyChanged
    {
    public:
        CoreInfo(Platform::String^ name, int maxFreq)
        {
            m_name = name;
            m_maxFreq = maxFreq;
            m_currentFreq = 0;
            m_usagePercent = 0;
        }

        // Property: Core Name
        property Platform::String^ CoreName
        {
            Platform::String^ get() { return m_name; }
        }

        // Property: Max Frequency
        property int MaxFrequency
        {
            int get() { return m_maxFreq; }
        }

        // Property: Current Frequency
        property int CurrentFrequency
        {
            int get() { return m_currentFreq; }
            void set(int value)
            {
                // Refresh the UI only if there are frequency changes
                if (m_currentFreq != value)
                {
                    m_currentFreq = value;
                    OnPropertyChanged("CurrentFrequency");
                    OnPropertyChanged("FrequencyText");
                }
            }
        }

        property Platform::String^ FrequencyText
        {
            Platform::String^ get() { return m_currentFreq.ToString() + " MHz"; }
        }

        // Property: Usage Percentage (0-100)
        property int UsagePercent
        {
            int get() { return m_usagePercent; }
            void set(int value)
            {
                if (m_usagePercent != value)
                {
                    m_usagePercent = value;
                    OnPropertyChanged("UsagePercent");
                    OnPropertyChanged("UsageText");
                }
            }
        }

        // Property: Usage Text
        property Platform::String^ UsageText
        {
            Platform::String^ get() { return m_usagePercent.ToString() + " %"; }
        }

        virtual event Windows::UI::Xaml::Data::PropertyChangedEventHandler^ PropertyChanged;

    private:
        void OnPropertyChanged(Platform::String^ propertyName)
        {
            PropertyChanged(this, ref new Windows::UI::Xaml::Data::PropertyChangedEventArgs(propertyName));
        }

        Platform::String^ m_name;
        int m_maxFreq;
        int m_currentFreq;
        int m_usagePercent; // Backing field for usage
    };

    public ref class MainPage sealed
    {
    public:
        MainPage();

        property Windows::Foundation::Collections::IObservableVector<CoreInfo^>^ CoresList
        {
            Windows::Foundation::Collections::IObservableVector<CoreInfo^>^ get() { return m_coresList; }
        }
        
        property Platform::String^ CpuDetails
        {
            Platform::String^ get() { return m_cpuDetails; }
        }

    private:
        void StartTimerAndRegisterHandler();
        void OnTick(Platform::Object^ sender, Platform::Object^ e);
        void OnRefreshSliderValueChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs^ e);
        void UpdateCoreData();
        DWORD GetCpuCoreCount();
        Platform::String^ GenerateCpuDetails();

        Platform::Collections::Vector<CoreInfo^>^ m_coresList;

        // Store previous times to calculate usage delta
        std::vector<CoreTimeData> m_prevCoreTimes;

        // Store previous cycle times to calculate frequency delta
        std::vector<uint64_t> m_prevCycleTimes;

        // High resolution timer variables
        int64_t m_prevTickTime;
        int64_t m_tickFrequency; 
        Windows::UI::Xaml::DispatcherTimer^ m_timer;
        int m_refreshIntervalMs;

        Platform::String^ m_cpuDetails;
    };
}
