#ifndef LIBBITCOIN_THREADPOOL_HPP
#define LIBBITCOIN_THREADPOOL_HPP

#include <functional>
#include <thread>

#include <boost/asio.hpp>

namespace libbitcoin {

using boost::asio::io_service;

/**
 * Threadpool represents a collection of running threads which can
 * be passed operations through their io_service.
 *
 * @code
 *  void task_foo();
 *
 *  // Run threadpool with a single running thread.
 *  threadpool pool(1);
 *  // Post operation to threadpool.
 *  // task_foo() will be called inside threadpool.
 *  pool.service.post(task_foo);
 * @endcode
 */
class threadpool
{
public:
    /**
     * Default constructor. Threadpool has no threads.
     * Call spawn() to start threads.
     */
    threadpool();
    /**
     * Convenience constructor. spawn()s number_threads threads.
     *
     * @param[in]   number_threads  Number of threads to spawn.
     */
    threadpool(size_t number_threads);
    ~threadpool();

    threadpool(const threadpool&) = delete;
    void operator=(const threadpool&) = delete;

    /**
     * Add a thread to this threadpool.
     */
    void spawn();

    /**
     * Stop the threadpool. All remaining operations on the queue are dropped.
     */
    void stop();
    /**
     * Finish executing all remaining operations in the queue.
     * Adding more operations keeps the threadpool running.
     */
    void shutdown();
    /**
     * Join all the threads in this threadpool with the current thread,
     * This method will hang until all the threads in this threadpool have
     * finished their running tasks and exited.
     *
     * Attempting to join() from within any of the threads owned by this
     * threadpool is invalid.
     *
     * WARNING: Do not call this within any of the threads owned by this
     * threadpool. Doing so will cause a resource deadlock and an
     * std::system_error exception will be thrown.
     *
     * @code
     *  // Threadpool with 4 running threads.
     *  threadpool pool(4);
     *  // Stop all 4 threads.
     *  pool.stop();
     *  // Wait for them to finish gracefully.
     *  pool.join();
     * @endcode
     */
    void join();

    template <typename... Args>
    void push(Args&&... args)
    {
        ios_.post(std::bind(std::forward<Args>(args)...));
    }

    template <typename... Args>
    void dispatch(Args&&... args)
    {
        ios_.dispatch(std::bind(std::forward<Args>(args)...));
    }

    /**
     * Underlying boost::io_service object.
     */
    io_service& service();
    /**
     * Underlying boost::io_service object.
     */
    const io_service& service() const;

private:
    io_service ios_;
    io_service::work* work_;
    std::vector<std::thread> threads_;
};


template <typename Handler>
struct wrapped_handler_impl
{
    Handler handler;
    io_service::strand& strand;

    template <typename... Args>
    void operator()(Args&&... args)
    {
        strand.dispatch(std::bind(
            handler, std::forward<Args>(args)...));
    }
};

/**
 * Convenience class for objects wishing to synchronize operations around
 * shared data.
 *
 * push() guarantees that any handlers passed to it will never execute
 * at the same time.
 *
 * queue() guarantees that any handlers passed to it will never execute
 * at the same time, and they will be called in sequential order.
 */
class async_strand
{
public:
    async_strand(threadpool& pool);

    /*
     * wrap() returns a new handler that guarantees that the handler it
     * encapsulates will never execute at the same time as another handler
     * passing through this class.
     *
     * @param[in]   handler     Handler to execute operation.
     * @code
     *  void handler();
     * @endcode
     */
    template <typename... Args>
    auto wrap(Args&&... args)
      -> wrapped_handler_impl<
            decltype(std::bind(std::forward<Args>(args)...))>
    {
        auto handler = std::bind(std::forward<Args>(args)...);
        return {handler, strand_};
    }

    /*
     * push() guarantees that any handlers passed to it will never execute
     * at the same time.
     *
     * @param[in]   handler     Handler to execute operation.
     * @code
     *  void handler();
     * @endcode
     */
    template <typename... Args>
    void push(Args&&... args)
    {
        ios_.post(strand_.wrap(std::bind(std::forward<Args>(args)...)));
    }

    /*
     * queue() guarantees that any handlers passed to it will never execute
     * at the same time, and they will be called in sequential order.
     *
     * Guarantees ordering.
     *
     * @param[in]   handler     Handler to execute operation.
     * @code
     *  void handler();
     * @endcode
     */
    template <typename... Args>
    void queue(Args&&... args)
    {
        strand_.post(std::bind(std::forward<Args>(args)...));
    }

private:
    io_service& ios_;
    io_service::strand strand_;
};

} // namespace libbitcoin

#endif

