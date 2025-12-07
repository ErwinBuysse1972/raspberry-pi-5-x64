#include <sstream>
#include <iomanip>
#include <algorithm>
#include <limits>
#include "Metrics.h"

using namespace MOW::Statistics;
P2Estimator::P2Estimator(double target)
{
    init(target);
}
void P2Estimator::init(double target)
{
    *this = P2Estimator{};
    p = target;
    f = { 0.0, p / 2, p, (p + 1.0) / 2.0, 1.0 };
}
bool P2Estimator::isWarm()const
{
    return init_count >= 5;
}
double P2Estimator::estimate() const
{
    if (!isWarm())
    {
        if (init_count == 0)
            return std::numeric_limits<double>::quiet_NaN();
        std::array<double, 5> tmp = init_buf;
        std::sort(tmp.begin(), tmp.begin() + static_cast<long>(init_count));
        // nearest-rank within warmup buffer
        std::size_t k = static_cast<std::size_t>(f[2] * (init_count - 1) + 0.5);
        if (k >= init_count)
            k = init_count - 1;
        return tmp[k];
    }
    return q[2];  // return the middle marker tracks the target percentile
}
void P2Estimator::addSample(double x)
{
    // Warm-up: collect first 5 samples
    if (!isWarm()) {
        init_buf[init_count++] = x;
        ++m_count;
        if (init_count == 5) finalizeWarmUp();
        return;
    }

    // After warmup
    ++m_count;

    // 1) Locate cell k and update end marker heights if needed
    int k;
    if (x < q[0]) {
        q[0] = x;
        k = 0;
    }
    else if (x < q[1]) {
        k = 0;
    }
    else if (x < q[2]) {
        k = 1;
    }
    else if (x < q[3]) {
        k = 2;
    }
    else if (x <= q[4]) {
        k = 3;
    }
    else {
        q[4] = x;
        k = 3;
    }

    // 2) Update integer positions n[i] for i>k
    for (int i = k + 1; i < 5; ++i) n[i] += 1;

    // 3) Desired positions advance by f[i] each new sample
    for (int i = 0; i < 5; ++i) np[i] += f[i];

    // 4) Adjust interior markers (i = 1..3) by at most 1 position
    for (int i = 1; i <= 3; ++i) {
        double d = np[i] - static_cast<double>(n[i]);
        int sign = (d >= 1.0) ? 1 : (d <= -1.0 ? -1 : 0);
        if (sign == 0) continue;

        // Parabolic prediction (Jain & Chlamtac P�)
        double n_im1 = static_cast<double>(n[i - 1]);
        double n_i = static_cast<double>(n[i]);
        double n_ip1 = static_cast<double>(n[i + 1]);

        double q_im1 = q[i - 1];
        double q_i = q[i];
        double q_ip1 = q[i + 1];

        //  Guard: if any denominator would be zero, skip (can't move this marker safely)
        double denom_all = (n_ip1 - n_im1);
        double denom_r = (n_ip1 - n_i);
        double denom_l = (n_i - n_im1);

        if (denom_all == 0.0 || denom_r == 0.0 || denom_l == 0.0)
        {
            n[i] = static_cast<std::size_t>(n_i + sign);
            continue;
        }
        // Parabolic prediction
        double q_parabolic = q_i + (sign / denom_all) *
            ((n_i - n_im1 + sign) * (q_ip1 - q_i) / denom_r +
                (n_ip1 - n_i - sign) * (q_i - q_im1) / denom_l);

        // Linear fallback (with guards)
        double q_linear;
        if (sign == 1) {
            double dr = static_cast<double>(n[i + 1] - n[i]);
            if (dr == 0.0) { n[i] = static_cast<std::size_t>(n_i + sign); continue; }
            q_linear = q_i + (q[i + 1] - q_i) / dr;
        }
        else {
            double dl = static_cast<double>(n[i - 1] - n[i]);
            if (dl == 0.0) { n[i] = static_cast<std::size_t>(n_i + sign); continue; }
            q_linear = q_i - (q[i - 1] - q_i) / dl;
        }

        double q_new = q_parabolic;
        if (q_new <= std::min(q_im1, q_ip1) || q_new >= std::max(q_im1, q_ip1)) {
            q_new = q_linear;
        }

        q[i] = q_new;
        n[i] = static_cast<std::size_t>(n_i + sign);
    }
}
void P2Estimator::finalizeWarmUp()
{
    // Sort initial 5 samples and place markers
    std::sort(init_buf.begin(), init_buf.end());
    for (int i = 0; i < 5; ++i)
    {
        q[i] = init_buf[static_cast<std::size_t>(i)];
        n[i] = static_cast<std::size_t>(i + 1); // positions 1..5
    }
    // Desired positions per sample count m_count (=5 now):
    // np[i] = 1 + (m_count - 1) * f[i]
    for (int i = 0; i < 5; ++i)
    {
        np[i] = 1.0 + static_cast<double>(m_count - 1) * f[i];
    }
}

void P2Set::init()
{
    p50.init(0.50);
    p95.init(0.95);
    p99.init(0.99);
}
void P2Set::addSample(double x)
{
    p50.addSample(x);
    p95.addSample(x);
    p99.addSample(x);
}
double P2Set::P50() const
{
    return p50.estimate();
}
double P2Set::P95() const
{
    return p95.estimate();
}
double P2Set::P99() const
{
    return p99.estimate();
}

MetricValue::MetricValue()
{
    reset();
}
bool MetricValue::addValue(double value)
{
    if (m_samples > 0)
    {
        double total = m_samples * m_avg + value;
        m_avg = total / static_cast<double>(m_samples + 1);
    }
    else
    {
        m_avg = value;
    }

    if (value < m_min) m_min = value;
    if (value > m_max) m_max = value;
    ++m_samples;
    p2.addSample(value);
    return true;
}
bool MetricValue::addFailure()
{
    ++m_failures;
    return true;
}
bool MetricValue::reset()
{
    m_min = std::numeric_limits<double>::max();
    m_max = std::numeric_limits<double>::min();
    m_avg = 0.0;
    m_failures = 0;
    m_samples = 0;
    p2.init();
    return true;
}
double MetricValue::getMax() const
{
    return m_max;
}
double MetricValue::getMin() const
{
    return m_min;
}
double MetricValue::getAvg() const
{
    return m_avg;
}
long long MetricValue::getSamples() const
{
    return m_samples;
}
long long MetricValue::getFailures() const
{
    return m_failures;
}
double MetricValue::getP50() const
{
    return p2.P50();
}
double MetricValue::getP95() const
{
    return p2.P95();
}
double MetricValue::getP99() const
{
    return p2.P99();
}

std::string MetricValue::ToString() const
{
    std::stringstream ss;
    ss << std::left
        << std::setw(15) << m_max
        << std::setw(15) << m_min
        << std::setw(15) << m_avg
        << std::setw(15) << p2.P50()
        << std::setw(15) << p2.P95()
        << std::setw(15) << p2.P99()
        << std::setw(15) << m_failures
        << std::setw(15) << m_samples
        << std::endl;
    return ss.str();
}

std::string MetricValue::ToString(const std::string& title) const
{
    std::stringstream ss;
    ss << std::left
        << std::setw(25) << title
        << std::setw(15) << m_max
        << std::setw(15) << m_min
        << std::setw(15) << m_avg
        << std::setw(15) << p2.P50()
        << std::setw(15) << p2.P95()
        << std::setw(15) << p2.P99()
        << std::setw(15) << m_failures
        << std::setw(15) << m_samples
        << std::endl;
    return ss.str();
}
