#include <cassert>

#include <numeric>

#include <util/span.hpp>

#include <memory/memory.hpp>
#include <memory/wrappers.hpp>

#include <cub/cub.cuh>

using namespace nest::mc;

void test_tricky();

int main() {
    test_tricky();
    return 0;
}

/// custom deleter for POD types that have been allocated on the device from the host.
template<typename T>
struct device_deleter {
    void operator()(T* p) const {
        std::cout << "Call delete for CUDA POD type object of size "
                << sizeof(T) << std::endl;
        cudaFree(p);
    }
};

/// unique pointer with custom deleter for device memory
template<typename T>
using device_pointer = std::unique_ptr<T, device_deleter<T>>;

/// returns a uniqe_ptr to a copy of POD type T in device memory
template<typename T>
device_pointer<T> copy_pod_to_device(const T& data) {
    T* data_on_device = nullptr;
    cudaMalloc(&data_on_device, sizeof(T));
    cudaMemcpy(data_on_device, &data, sizeof(T), cudaMemcpyHostToDevice);
    return device_pointer<T>{data_on_device};
}

/// returns a value of POD type stored on device
template<typename T>
T copy_pod_from_device(const T* data) {
    T data_on_host;
    cudaMemcpy(&data_on_host, data, sizeof(T), cudaMemcpyDeviceToHost);
    return data_on_host;
}

namespace kernels {

// functor used to keep track of the prefix used in calculating the offset using
// cub::BlockScan::ExclusiveSum
struct prefix_accumulator {
    int running_total;

    __device__
    prefix_accumulator(int i): running_total(i) {}

    __device__
    int operator()(int offset) {
        int old_total = running_total;
        running_total += offset;
        return old_total;
    }
};


template <typename T, int THREADS>
struct gpu_stack {
    using BlockScan = cub::BlockScan<int, THREADS>;
    using temp_storage = typename BlockScan::TempStorage;

    int capacity_;
    int size_;
    T* data_;
    prefix_accumulator prefix_;
    temp_storage& shared_;

    __device__
    gpu_stack(int capacity, int size, T* data, temp_storage& shared):
        capacity_(capacity),
        size_(size),
        data_(data),
        prefix_(size),
        shared_(shared)
    {}

    __device__
    void push_back(const T& value, bool do_push) {
        int position;
        int contribution = do_push ? 1: 0;
        BlockScan(shared_).ExclusiveSum(contribution, position, prefix_);
        __syncthreads();

        // only push back if the capacity of the stack would not
        // be exceeded by doing so.
        if (do_push && position<capacity_) {
            printf("%d : %d -- writing at %d\n", int(threadIdx.x), contribution, position);
            data_[position] = value;
        }

        // It is possible that size_>capacity_.
        // In this case, only capacity_ entries are stored, and additional
        // values are lost. The size_ will contain the total number of attempts
        // to push, so that the caller can determine how far the capacity was
        // exceeded.  It is the responsbility of the caller to check whether
        // capacity has been exceeded and act accordingly.
        size_ = prefix_.running_total;
    }

    __device__
    int size() const {
        return size_;
    }

    __device__
    int capacity() const {
        return capacity_;
    }
};

template <typename T, typename I, typename Stack, int THREADS>
__global__
void test(
    float t, float t_prev, int size,
    typename Stack::gpu_proxy* stack_proxy,
    uint8_t* is_spiking,
    const T* values,
    T* prev_values,
    const T* thresholds)
{
    int tid = threadIdx.x;

    int num_blocks = (size+THREADS-1) / THREADS;

    // TODO : the Buffer type and using its value_type is ugly as sin
    //        i.e. the buffer->stack abstraction still isn't quite right
    //using stack_type = gpu_stack<typename Buffer::value_type, THREADS>;
    using stack_type = typename Stack::gpu_stack;
    __shared__ typename stack_type::temp_storage shared_store;

    //stack_type stack(buffer->size, buffer->size(), buffer->data, shared_store);
    stack_type stack = stack_proxy->stack(shared_store);

    for (int i=tid; i<num_blocks*THREADS; i+=THREADS) {
        bool crossed = false;
        float crossing_time;
        if (i<size) {
            // Test for threshold crossing
            const auto v_prev = prev_values[i];
            const auto v = values[i];
            const auto thresh = thresholds[i];

            if (!is_spiking[i]) {
                if (v>=thresh) {
                    // The threshold has been passed, so estimate the time using
                    // linear interpolation
                    auto pos = (thresh - v_prev)/(v - v_prev);
                    crossing_time = t_prev + pos*(t - t_prev);

                    is_spiking[i] = 1;
                    crossed = true;
                }
            }
            else if (v<thresh) {
                is_spiking[i]=0;
            }

            prev_values[i] = values[i];
        }

        stack.push_back({i, crossing_time}, crossed);
    }

    // the first thread updates the buffer size
    if (tid==0) {
        stack_proxy->size = stack.size();
    }
}

}// namespace kernels

template <typename T, int THREADS, template <typename, int> class GPUStack>
struct stack {
    using value_type = T;
    using gpu_stack = GPUStack<value_type, THREADS>;

    struct gpu_proxy {
        using value_type = value_type;
        value_type* data;
        int capacity;
        int size;

        __host__
        gpu_proxy(value_type* ptr, int capacity, int size):
            data(ptr),
            capacity(capacity),
            size(size)
        {}

        __device__
        gpu_stack stack(typename gpu_stack::temp_storage& shared) {
            return {capacity, size, data, shared};
        }
    };

    int size_;
    memory::device_vector<value_type> data_;
    mutable device_pointer<gpu_proxy> proxy_;

    stack():
        stack(0)
    {}

    stack(int cap):
        data_(cap),
        size_(0),
        proxy_(new gpu_proxy(data_.data(), capacity(), size()))
    {}

    int capacity() const {
        return data_.size();
    }

    int size() const {
        update_host();
        return size_;
    }

    bool is_overflow() const {
        update_host();
        return size()>capacity();
    }

    gpu_proxy* on_gpu() {
        return proxy_.get();
    }

    std::vector<T> get_stack() const {
        update_host();
        // TODO : return the values copied out from gpu
        return {};
    }

private:

    void update_host() const {
        // TODO :
        //  * copy proxy_ data back to host
        //  * update size_
    }
};


//
// type that manages threshold crossing checker on the GPU
//
template <typename T, typename I>
struct threshold_watcher {
    struct crossing_type {
        I index;
        T time;
    };

    using stack_type = stack<crossing_type, 128, kernels::gpu_stack>;

    template <typename U>
    using array = memory::device_vector<U>;

    template <typename U>
    using array_view = typename array<U>::const_view_type;

    threshold_watcher(array_view<T> values, const std::vector<T>& thresholds, T t):
        values_(values),
        prev_values_(values),
        thresholds_(memory::make_const_view(thresholds)),
        is_spiking_(size(), 0),
        t_prev_(t)
    {
        // Initialize initial state in the is_spiking array.
        // This is performed on the host for convenience, because it is just
        // done once during setup.
        auto v = memory::on_host(values);
        auto spiking = std::vector<uint8_t>(values.size());
        for (auto i: util::make_span(0u, values.size())) {
            spiking[i] = v[i] < thresholds[i] ? 0 : 1;
        }
        is_spiking_ = memory::on_gpu(spiking);

        // copy the buffer information to device memory
    }

    void test(T t) {
        EXPECTS(t_prev_<t);

        kernels::test<T, I, stack_type, 64><<<1, 64>>>(
            t, t_prev_, size(),
            stack_.on_gpu(),
            is_spiking_.data(),
            values_.data(),
            prev_values_.data(),
            thresholds_.data());

        t_prev_ = t;
    }

    std::vector<crossing_type> get_crossings() const {
        return stack_.get_stack();
    }

    std::size_t size() const {
        return thresholds_.size();
    }

    array_view<T> values_;
    array<T> prev_values_;
    array<T> thresholds_;
    array<uint8_t> is_spiking_;
    stack_type stack_;
    T t_prev_;
};

void test_tricky() {
    using watcher = threshold_watcher<float, int>;

    constexpr auto N = 24;
    auto values = memory::device_vector<float>(N, 0);
    auto thresholds = std::vector<float>(N, 0.5);

    EXPECTS(values.size()==N);
    EXPECTS(thresholds.size()==N);

    auto w = watcher(values, thresholds, 0);

    memory::fill(values, 1); w.test(1);
    memory::fill(values, 0); w.test(2);
    memory::fill(values, 2); w.test(3);

    auto crossings = w.get_crossings();
    std::cout << "CROSSINGS (" << crossings.size() << ")\n";
    for(auto c: crossings) std::cout << "  " << c.index << " -- " << c.time << "\n";
}

