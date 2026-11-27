#pragma once

#include <array>
#include <atomic>
#include <cstddef>

namespace harmonia::core {

/// Fila circular lock-free Single-Producer Single-Consumer (SPSC) sem alocacao dinamica.
/// Capacidade deve ser potencia de 2 >= 2.
template <typename T, std::size_t Capacity>
class SpscRingBuffer {
    static_assert(Capacity >= 2 && (Capacity & (Capacity - 1)) == 0,
                  "SpscRingBuffer Capacity must be a power of 2 >= 2");

public:
    constexpr SpscRingBuffer() noexcept = default;

    SpscRingBuffer(const SpscRingBuffer&) = delete;
    SpscRingBuffer& operator=(const SpscRingBuffer&) = delete;

    [[nodiscard]] constexpr std::size_t capacity() const noexcept {
        return Capacity - 1u; // Um slot reservado para distinguir cheio de vazio
    }

    bool push(const T& item) noexcept {
        const std::size_t head = head_.load(std::memory_order_relaxed);
        const std::size_t next_head = (head + 1u) & kMask;
        if (next_head == tail_.load(std::memory_order_acquire)) {
            return false; // Cheio
        }
        buffer_[head] = item;
        head_.store(next_head, std::memory_order_release);
        return true;
    }

    bool pop(T& out_item) noexcept {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) {
            return false; // Vazio
        }
        out_item = buffer_[tail];
        tail_.store((tail + 1u) & kMask, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool empty() const noexcept {
        return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
    }

    [[nodiscard]] std::size_t size_approx() const noexcept {
        const std::size_t head = head_.load(std::memory_order_acquire);
        const std::size_t tail = tail_.load(std::memory_order_acquire);
        return (head - tail) & kMask;
    }

    void reset() noexcept {
        head_.store(0, std::memory_order_relaxed);
        tail_.store(0, std::memory_order_relaxed);
    }

private:
    static constexpr std::size_t kMask = Capacity - 1u;

    alignas(64) std::atomic<std::size_t> head_{0};
    alignas(64) std::atomic<std::size_t> tail_{0};
    std::array<T, Capacity> buffer_{};
};

} // namespace harmonia::core