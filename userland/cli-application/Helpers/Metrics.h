#pragma once
#include <string>
#include <array>
namespace MOW::Statistics
{
    //More info on the P² algorithm used can be found here: https://aakinshin.net/posts/p2-quantile-estimator-intro/
    struct P2Estimator
    {
        double p;

        // streaming state
        std::size_t m_count{ 0 };                   // total samples seen
        std::array<double, 5> f{};                  // marker quantiles: [0, p/2, p, (1+p)/2, 1]
        std::array<double, 5> q{};                  // marker heights
        std::array<std::size_t, 5> n{};             // marker positions (integers)
        std::array<double, 5> np{};                 // marker positions (integers)
        std::array<double, 5> init_buf{};           // first 5 samples
        std::size_t init_count{ 0 };

        P2Estimator() = default;
        explicit P2Estimator(double target);
        void init(double target);
        bool isWarm()const;
        double estimate() const;
        void addSample(double x);
    private:
        void finalizeWarmUp();
    };

    struct P2Set
    {

        P2Estimator p50{ 0.50 };
        P2Estimator p95{ 0.95 };
        P2Estimator p99{ 0.99 };

        void init();
        void addSample(double x);
        double P50() const;
        double P95() const;
        double P99() const;
    };


    class MetricValue
    {
    public:
        MetricValue();
        virtual ~MetricValue() = default;

        bool addValue(double value);
        bool addFailure();
        bool reset();

        double getMax() const;
        double getMin() const;
        double getAvg() const;
        double getP50() const;
        double getP95() const;
        double getP99() const;
        long long getSamples() const;
        long long getFailures() const;

        std::string ToString() const;
        std::string ToString(const std::string& title) const;

    private:
        double m_max;
        double m_min;
        double m_avg;
        long long m_failures;
        long long m_samples;
        P2Set p2{};
    };
} // namespace MathModel
