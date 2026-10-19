#pragma once

#include <eva/detail/binary_value.hpp>

#include <algorithm>
#include <cstddef>
#include <bit>
#include <expected>
#include <span>
#include <system_error>


namespace eva
{
    // class `binary_reader`

    template<std::endian endian = std::endian::little>
    class binary_reader final
    {
    public:
        constexpr binary_reader() noexcept = default;

        constexpr explicit binary_reader(std::span<const std::byte> span) noexcept
            : bytes_(span)
        {}
        
        binary_reader(const void* data, std::size_t size) noexcept
            : binary_reader(std::span<const std::byte>{static_cast<const std::byte*>(data), size})
        {}
    
    public:
        [[nodiscard]] constexpr std::expected<void, std::errc> read(std::span<std::byte> destination) noexcept
        {
            auto result = peek(destination);
            if (!result)
                return result;

            position_ += destination.size();
            return {};
        }

        template<typename T>
            requires detail::binary_value<T>
        [[nodiscard]] constexpr std::expected<T, std::errc> read_value() noexcept
        {
            auto result = peek_value<T>();
            if (result)
                position_ += sizeof(T);

            return result;
        }

        [[nodiscard]] constexpr std::expected<std::span<const std::byte>, std::errc> read_view(std::size_t size) noexcept
        {
            auto result = peek_view(size);
            if (result)
                position_ += size;

            return result;
        }
    
    public:
        [[nodiscard]] constexpr std::expected<void, std::errc> peek(std::span<std::byte> destination) const noexcept
        {
            auto source = peek_view(destination.size());
            if (!source)
                return std::unexpected(source.error());

            std::ranges::copy(*source, destination.begin());

            return {};
        }

        template<typename T>
            requires detail::binary_value<T>
        [[nodiscard]] constexpr std::expected<T, std::errc> peek_value() const noexcept
        {
            auto source = peek_view(sizeof(T));
            if (!source)
                return std::unexpected(source.error());

            std::byte representation[sizeof(T)];
            if constexpr (endian == std::endian::native)
                std::ranges::copy(*source, representation);
            else
                std::ranges::reverse_copy(*source, representation);

            return std::bit_cast<T>(representation);
        }

        [[nodiscard]] constexpr std::expected<std::span<const std::byte>, std::errc> peek_view(std::size_t size) const noexcept
        {
            if (size > remaining())
                return std::unexpected(std::errc::result_out_of_range);

            return bytes_.subspan(position_, size);
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
        [[nodiscard]] constexpr std::span<const std::byte> span() const noexcept
        {
            return bytes_;
        }

        [[nodiscard]] constexpr const std::byte* data() const noexcept
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
        std::span<const std::byte> bytes_;
        std::size_t position_ = 0;
    };
}
