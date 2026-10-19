#pragma once

#include <eva/detail/binary_value.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <expected>
#include <span>
#include <system_error>


namespace eva
{
    // class `binary_writer`

    template<std::endian endian = std::endian::little>
    class binary_writer final
    {
    public:
        constexpr binary_writer() noexcept = default;

        constexpr explicit binary_writer(std::span<std::byte> span) noexcept
            : bytes_(span)
        {}

        binary_writer(void* data, std::size_t size) noexcept
            : binary_writer(std::span<std::byte>{static_cast<std::byte*>(data), size})
        {}

    public:
        [[nodiscard]] constexpr std::expected<void, std::errc> write(std::span<const std::byte> source) noexcept
        {
            auto destination = destination_view(source.size());
            if (!destination)
                return std::unexpected(destination.error());

            std::ranges::copy(source, destination->begin());
            position_ += source.size();
            return {};
        }

        template<typename T>
            requires detail::binary_value<T>
        [[nodiscard]] constexpr std::expected<void, std::errc> write_value(T value) noexcept
        {
            auto destination = destination_view(sizeof(T));
            if (!destination)
                return std::unexpected(destination.error());

            auto representation = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
            if constexpr (endian == std::endian::native)
                std::ranges::copy(representation, destination->begin());
            else
                std::ranges::reverse_copy(representation, destination->begin());

            position_ += sizeof(T);
            return {};
        }

        [[nodiscard]] constexpr std::expected<std::span<std::byte>, std::errc> write_view(std::size_t size) noexcept
        {
            auto result = destination_view(size);
            if (result)
                position_ += size;

            return result;
        }

    public:
        [[nodiscard]] constexpr std::expected<void, std::errc> seek(std::size_t position) noexcept
        {
            if (position > bytes_.size())
                return std::unexpected(std::errc::result_out_of_range);

            position_ = position;
            return {};
        }

        [[nodiscard]] constexpr std::expected<void, std::errc> skip(std::size_t size) noexcept
        {
            if (size > remaining())
                return std::unexpected(std::errc::result_out_of_range);

            position_ += size;
            return {};
        }

    public:
        [[nodiscard]] constexpr std::span<std::byte> span() const noexcept
        {
            return bytes_;
        }

        [[nodiscard]] constexpr std::byte* data() const noexcept
        {
            return bytes_.data();
        }

        [[nodiscard]] constexpr std::size_t size() const noexcept
        {
            return bytes_.size();
        }

        [[nodiscard]] constexpr std::size_t position() const noexcept
        {
            return position_;
        }

        [[nodiscard]] constexpr std::size_t remaining() const noexcept
        {
            return bytes_.size() - position_;
        }

        [[nodiscard]] constexpr bool eof() const noexcept
        {
            return position_ == bytes_.size();
        }

    private:
        [[nodiscard]] constexpr std::expected<std::span<std::byte>, std::errc> destination_view(std::size_t size) const noexcept
        {
            if (size > remaining())
                return std::unexpected(std::errc::result_out_of_range);

            return bytes_.subspan(position_, size);
        }

    private:
        std::span<std::byte> bytes_;
        std::size_t position_ = 0;
    };
}
