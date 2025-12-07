#pragma once
#include <memory>
#include "../Tracer/ctracer.h"
#include "SBPio.h"

namespace SB::RPI5::Sensors
{
    class CDhct11
    {
        public:
            CDhct11(std::shared_ptr<CTracer> tracer, int pin);
            virtual ~CDhct11();

            bool Read(int& temperature, int&humidity, std::vector<std::string>& errors);

        private:
            std::shared_ptr<CTracer> m_trace;
            std::unique_ptr<SBPio>   m_pio;
            int m_pin;
    };
}