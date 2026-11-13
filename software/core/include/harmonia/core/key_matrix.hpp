#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace harmonia::core {

/// Coordenada lógica (linha, coluna) de uma tecla na matriz.
struct KeyCoord {
    std::uint8_t row{0};
    std::uint8_t col{0};

    constexpr bool operator==(const KeyCoord& other) const noexcept {
        return row == other.row && col == other.col;
    }

    constexpr bool operator!=(const KeyCoord& other) const noexcept {
        return !(*this == other);
    }
};

/// Transição estável detectada após o debounce.
enum class KeyTransition : std::uint8_t {
    None = 0,
    Pressed,
    Released
};

/// Matriz lógica configurável em tempo de execução até os limites MaxRows x MaxCols,
/// sem alocação dinâmica e independente de GPIOs físicos.
template <std::size_t MaxRows, std::size_t MaxCols>
class KeyMatrixState {
public:
    static constexpr std::size_t kMaxKeys = MaxRows * MaxCols;

    constexpr KeyMatrixState() noexcept = default;

    constexpr explicit KeyMatrixState(std::uint8_t rows, std::uint8_t cols) noexcept {
        configure(rows, cols);
    }

    /// Configura dimensões ativas (retorna false se excederem os limites ou forem zero).
    constexpr bool configure(std::uint8_t rows, std::uint8_t cols) noexcept {
        if (rows == 0 || cols == 0 || rows > MaxRows || cols > MaxCols) {
            return false;
        }
        rows_ = rows;
        cols_ = cols;
        clear();
        return true;
    }

    constexpr void clear() noexcept {
        pressed_.fill(false);
    }

    [[nodiscard]] constexpr std::uint8_t rows() const noexcept { return rows_; }
    [[nodiscard]] constexpr std::uint8_t cols() const noexcept { return cols_; }
    [[nodiscard]] constexpr std::size_t key_count() const noexcept {
        return static_cast<std::size_t>(rows_) * static_cast<std::size_t>(cols_);
    }

    [[nodiscard]] constexpr bool is_valid_coord(std::uint8_t row, std::uint8_t col) const noexcept {
        return row < rows_ && col < cols_;
    }

    /// Converte (row, col) em índice linear estável (row-major).
    [[nodiscard]] constexpr std::optional<std::size_t> index_of(std::uint8_t row, std::uint8_t col) const noexcept {
        if (!is_valid_coord(row, col)) {
            return std::nullopt;
        }
        return static_cast<std::size_t>(row) * static_cast<std::size_t>(cols_) +
               static_cast<std::size_t>(col);
    }

    /// Converte índice linear estável em coordenada (row, col).
    [[nodiscard]] constexpr std::optional<KeyCoord> coord_of(std::size_t index) const noexcept {
        if (index >= key_count()) {
            return std::nullopt;
        }
        return KeyCoord{
            static_cast<std::uint8_t>(index / cols_),
            static_cast<std::uint8_t>(index % cols_)
        };
    }

    /// Define estado lógico de uma tecla (para simulação ou preenchimento por driver de hardware).
    constexpr bool set_key(std::uint8_t row, std::uint8_t col, bool pressed) noexcept {
        const auto idx = index_of(row, col);
        if (!idx.has_value()) {
            return false;
        }
        pressed_[*idx] = pressed;
        return true;
    }

    [[nodiscard]] constexpr bool get_key(std::uint8_t row, std::uint8_t col) const noexcept {
        const auto idx = index_of(row, col);
        if (!idx.has_value()) {
            return false;
        }
        return pressed_[*idx];
    }

    [[nodiscard]] constexpr bool get_by_index(std::size_t index) const noexcept {
        if (index >= key_count()) {
            return false;
        }
        return pressed_[index];
    }

private:
    std::uint8_t rows_{static_cast<std::uint8_t>(MaxRows)};
    std::uint8_t cols_{static_cast<std::uint8_t>(MaxCols)};
    std::array<bool, kMaxKeys> pressed_{};
};

} // namespace harmonia::core
