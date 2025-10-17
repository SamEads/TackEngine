#include <math.h>
#include "timer.h"

Timer::Timer(float tps) {
    setTickRate(tps);
}

void Timer::update() {
    long long time = sysTime();
    double delta = (time - m_lastSync) / static_cast<double>(m_tickLength);

    m_elapsedTicks = static_cast<int>(floor(delta));
    m_alpha = static_cast<float>(delta - static_cast<double>(m_elapsedTicks));

    m_lastSync += static_cast<long long>(static_cast<double>(m_elapsedTicks) * m_tickLength);
}

void Timer::setTickRate(double tps) {
    this->m_tps = tps;
    m_tickLength = 1000.0 / tps;
    m_lastSync = sysTime();
}