#include <chrono>

class Timer {
public:
    Timer() = default;
    Timer(float tps);
    void update();
    void setTickRate(double tps);

    const int getTickCount() const { return m_elapsedTicks; }
    const float getAlpha() const { return m_alpha; }

private:
    int m_elapsedTicks = 0;

    double m_tps = 0.0;
    double m_tickLength = 0.0;
    long long m_lastSync = 0;

    float m_alpha = 0.0f;
};

inline long long sysTime() {
    using namespace std::chrono;
    return duration_cast<microseconds>(system_clock::now().time_since_epoch()).count() / 1000ll;
}