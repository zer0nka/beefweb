#pragma once

#include "work_queue.hpp"
#include "timers.hpp"

#include <boost/asio.hpp>

namespace msrv {

class AsioWorkQueue final : public ExternalWorkQueue
{
public:
    AsioWorkQueue(boost::asio::io_context* context);
    virtual ~AsioWorkQueue();

protected:
    virtual void schedule() override;

private:
    boost::asio::io_context* context_;
};

class AsioTimer final : public Timer
{
public:
    AsioTimer(boost::asio::io_context* context);
    virtual ~AsioTimer();

    virtual TimerState state() const override { return state_; }
    virtual DurationMs period() const override { return period_; }

    virtual void setCallback(TimerCallback callback) override { callback_ = std::move(callback); }

    virtual void runOnce(DurationMs delay) override;
    virtual void runPeriodic(DurationMs period) override;
    virtual void stop() override;

private:
    void schedule(DurationMs delay);
    void onComplete(const boost::system::error_code& error);

    boost::asio::deadline_timer timer_;
    TimerState state_;
    DurationMs period_;
    TimerCallback callback_;
};

class AsioTimerFactory final : public TimerFactory
{
public:
    AsioTimerFactory(boost::asio::io_context* context);
    virtual ~AsioTimerFactory();
    virtual TimerPtr createTimer() override;

private:
    boost::asio::io_context* context_;
};

}
