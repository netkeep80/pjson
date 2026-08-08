#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <span>
#include <string_view>

#include "pjson/node.h"
#include "pmm/parray.h"
#include "pmm/pstring.h"

namespace pjson
{

// @req FR-002 — string and binary are first-class pjson value types.
// @req FR-014 — the outer semantic discriminator remains PMM NodeType metadata;
// the compact pjson payload stores only the native PMM backing-object offset.
template <typename Manager> using string_storage = pmm::pstring<Manager>;
template <typename Manager> using binary_storage = pmm::parray<std::uint8_t, Manager>;

template <typename Manager> bool is_string( node_ref<Manager> node ) noexcept
{
    const auto type = node_type_of<Manager>( node );
    return type.has_value() && *type == node_type::string_value;
}

template <typename Manager> bool is_binary( node_ref<Manager> node ) noexcept
{
    const auto type = node_type_of<Manager>( node );
    return type.has_value() && *type == node_type::binary_value;
}

namespace detail
{

template <typename T, typename Manager> struct arena_source
{
    static constexpr std::size_t npos = ( std::numeric_limits<std::size_t>::max )();

    const T*    raw       = nullptr;
    std::size_t arena_off = npos;
    bool        valid     = true;

    static arena_source capture( const T* source, std::size_t count ) noexcept
    {
        arena_source result;
        result.raw = source;
        if ( count == 0 )
            return result;
        if ( source == nullptr || count > ( std::numeric_limits<std::size_t>::max )() / sizeof( T ) )
        {
            result.valid = false;
            return result;
        }

        const auto* base = Manager::backend().base_ptr();
        if ( base == nullptr )
            return result;

        const auto begin = reinterpret_cast<std::uintptr_t>( base );
        const auto addr  = reinterpret_cast<std::uintptr_t>( source );
        const auto bytes = Manager::backend().total_size();
        if ( addr < begin || addr - begin >= bytes )
            return result;

        const std::size_t off          = static_cast<std::size_t>( addr - begin );
        const std::size_t source_bytes = count * sizeof( T );
        if ( source_bytes > bytes - off )
        {
            result.valid = false;
            return result;
        }
        result.arena_off = off;
        return result;
    }

    const T* resolve() const noexcept
    {
        if ( arena_off == npos )
            return raw;
        const auto* base = Manager::backend().base_ptr();
        return base ? reinterpret_cast<const T*>( base + arena_off ) : nullptr;
    }
};

template <typename Storage, typename Manager>
pmm::pptr<Storage, Manager> backing_ref( node_ref<Manager> node, node_type expected ) noexcept
{
    const auto type = node_type_of<Manager>( node );
    if ( !type.has_value() || *type != expected )
        return {};
    const auto* payload = Manager::resolve( node );
    if ( payload == nullptr )
        return {};
    pmm::pptr<Storage, Manager> backing( payload->storage_offset );
    return Manager::get_node_type( backing ) == pmm::node_type_for_v<Storage> ? backing : pmm::pptr<Storage, Manager>{};
}

template <typename Storage, typename Manager>
void cleanup_backing( pmm::pptr<Storage, Manager> backing ) noexcept
{
    if ( auto* state = Manager::resolve( backing ) )
        state->free_data();
    Manager::template destroy_typed<Storage>( backing );
}

template <typename Storage, typename Manager>
node_ref<Manager> bind_backing( node_type type, pmm::pptr<Storage, Manager> backing ) noexcept
{
    auto node = allocate_node<Manager>( type );
    if ( node.is_null() )
        return {};
    auto* payload = Manager::resolve( node );
    if ( payload == nullptr )
    {
        deallocate_node<Manager>( node );
        return {};
    }
    payload->storage_offset = backing.offset();
    return node;
}

template <typename Manager>
bool write_binary( pmm::pptr<binary_storage<Manager>, Manager> backing, const std::uint8_t* source,
                   std::size_t count, bool append ) noexcept
{
    auto* state = Manager::resolve( backing );
    if ( state == nullptr )
        return false;

    const std::size_t old_size = state->size();
    const std::size_t base     = append ? old_size : 0;
    if ( count == 0 )
        return append || state->resize( 0 );
    if ( source == nullptr || base > ( std::numeric_limits<std::uint32_t>::max )() ||
         count > ( std::numeric_limits<std::uint32_t>::max )() - base )
        return false;

    constexpr std::size_t npos = ( std::numeric_limits<std::size_t>::max )();
    std::size_t self_off = npos;
    if ( const auto* old_data = state->data() )
    {
        const auto begin = reinterpret_cast<std::uintptr_t>( old_data );
        const auto addr  = reinterpret_cast<std::uintptr_t>( source );
        if ( addr >= begin && addr - begin <= state->capacity() )
        {
            const auto off = static_cast<std::size_t>( addr - begin );
            if ( off >= old_size || count > old_size - off )
                return false;
            self_off = off;
        }
    }

    const auto arena = self_off == npos ? arena_source<std::uint8_t, Manager>::capture( source, count )
                                         : arena_source<std::uint8_t, Manager>{};
    if ( self_off == npos && !arena.valid )
        return false;

    if ( !state->resize( base + count ) )
        return false;
    state = Manager::resolve( backing );
    if ( state == nullptr )
        return false;
    auto* destination = state->data();
    if ( destination == nullptr )
        return false;

    if ( self_off != npos )
        source = destination + self_off;
    else
        source = arena.resolve();
    if ( source == nullptr )
        return false;

    std::memmove( destination + base, source, count );
    return true;
}

template <typename Storage, typename Manager>
bool destroy_backed( node_ref<Manager> node, node_type expected ) noexcept
{
    const auto backing = backing_ref<Storage, Manager>( node, expected );
    if ( backing.is_null() )
        return false;
    cleanup_backing<Storage, Manager>( backing );
    deallocate_node<Manager>( node );
    return true;
}

} // namespace detail

template <typename Manager>
node_ref<Manager> make_string( const char* value, std::size_t length ) noexcept
{
    const auto source = detail::arena_source<char, Manager>::capture( value, length );
    if ( !source.valid )
        return {};

    using Storage = string_storage<Manager>;
    auto backing = Manager::template create_typed<Storage>();
    if ( backing.is_null() )
        return {};
    auto* state = Manager::resolve( backing );
    if ( state == nullptr || !state->assign( source.resolve(), length ) )
    {
        detail::cleanup_backing<Storage, Manager>( backing );
        return {};
    }

    auto node = detail::bind_backing<Storage, Manager>( node_type::string_value, backing );
    if ( node.is_null() )
        detail::cleanup_backing<Storage, Manager>( backing );
    return node;
}

template <typename Manager>
node_ref<Manager> make_string( std::string_view value ) noexcept
{
    return make_string<Manager>( value.data(), value.size() );
}

template <typename Manager>
std::optional<std::string_view> get_string( node_ref<Manager> node ) noexcept
{
    using Storage = string_storage<Manager>;
    const auto backing = detail::backing_ref<Storage, Manager>( node, node_type::string_value );
    const auto* state = Manager::resolve( backing );
    if ( state == nullptr )
        return std::nullopt;
    return std::string_view( state->c_str(), state->size() );
}

template <typename Manager>
bool set_string( node_ref<Manager> node, const char* value, std::size_t length ) noexcept
{
    using Storage = string_storage<Manager>;
    const auto backing = detail::backing_ref<Storage, Manager>( node, node_type::string_value );
    auto* state = Manager::resolve( backing );
    return state != nullptr && state->assign( value, length );
}

template <typename Manager>
bool set_string( node_ref<Manager> node, std::string_view value ) noexcept
{
    return set_string<Manager>( node, value.data(), value.size() );
}

template <typename Manager>
bool append_string( node_ref<Manager> node, const char* value, std::size_t length ) noexcept
{
    using Storage = string_storage<Manager>;
    const auto backing = detail::backing_ref<Storage, Manager>( node, node_type::string_value );
    auto* state = Manager::resolve( backing );
    return state != nullptr && state->append( value, length );
}

template <typename Manager>
bool append_string( node_ref<Manager> node, std::string_view value ) noexcept
{
    return append_string<Manager>( node, value.data(), value.size() );
}

template <typename Manager>
node_ref<Manager> clone_string( node_ref<Manager> source ) noexcept
{
    const auto value = get_string<Manager>( source );
    return value.has_value() ? make_string<Manager>( *value ) : node_ref<Manager>{};
}

template <typename Manager>
bool destroy_string( node_ref<Manager> node ) noexcept
{
    return detail::destroy_backed<string_storage<Manager>, Manager>( node, node_type::string_value );
}

template <typename Manager>
node_ref<Manager> make_binary( const std::uint8_t* value, std::size_t size ) noexcept
{
    const auto source = detail::arena_source<std::uint8_t, Manager>::capture( value, size );
    if ( !source.valid )
        return {};

    using Storage = binary_storage<Manager>;
    auto backing = Manager::template create_typed<Storage>();
    if ( backing.is_null() )
        return {};
    if ( !detail::write_binary<Manager>( backing, source.resolve(), size, false ) )
    {
        detail::cleanup_backing<Storage, Manager>( backing );
        return {};
    }

    auto node = detail::bind_backing<Storage, Manager>( node_type::binary_value, backing );
    if ( node.is_null() )
        detail::cleanup_backing<Storage, Manager>( backing );
    return node;
}

template <typename Manager>
node_ref<Manager> make_binary( std::span<const std::uint8_t> value ) noexcept
{
    return make_binary<Manager>( value.data(), value.size() );
}

template <typename Manager>
std::optional<std::span<const std::uint8_t>> get_binary( node_ref<Manager> node ) noexcept
{
    using Storage = binary_storage<Manager>;
    const auto backing = detail::backing_ref<Storage, Manager>( node, node_type::binary_value );
    const auto* state = Manager::resolve( backing );
    if ( state == nullptr )
        return std::nullopt;
    if ( state->empty() )
        return std::span<const std::uint8_t>{};
    const auto* data = state->data();
    return data ? std::optional<std::span<const std::uint8_t>>( std::span<const std::uint8_t>( data, state->size() ) )
                : std::nullopt;
}

template <typename Manager>
bool set_binary( node_ref<Manager> node, const std::uint8_t* value, std::size_t size ) noexcept
{
    using Storage = binary_storage<Manager>;
    const auto backing = detail::backing_ref<Storage, Manager>( node, node_type::binary_value );
    return !backing.is_null() && detail::write_binary<Manager>( backing, value, size, false );
}

template <typename Manager>
bool set_binary( node_ref<Manager> node, std::span<const std::uint8_t> value ) noexcept
{
    return set_binary<Manager>( node, value.data(), value.size() );
}

template <typename Manager>
bool append_binary( node_ref<Manager> node, const std::uint8_t* value, std::size_t size ) noexcept
{
    using Storage = binary_storage<Manager>;
    const auto backing = detail::backing_ref<Storage, Manager>( node, node_type::binary_value );
    return !backing.is_null() && detail::write_binary<Manager>( backing, value, size, true );
}

template <typename Manager>
bool append_binary( node_ref<Manager> node, std::span<const std::uint8_t> value ) noexcept
{
    return append_binary<Manager>( node, value.data(), value.size() );
}

template <typename Manager>
bool resize_binary( node_ref<Manager> node, std::size_t size ) noexcept
{
    using Storage = binary_storage<Manager>;
    const auto backing = detail::backing_ref<Storage, Manager>( node, node_type::binary_value );
    auto* state = Manager::resolve( backing );
    return state != nullptr && state->resize( size );
}

template <typename Manager>
node_ref<Manager> clone_binary( node_ref<Manager> source ) noexcept
{
    const auto value = get_binary<Manager>( source );
    return value.has_value() ? make_binary<Manager>( *value ) : node_ref<Manager>{};
}

template <typename Manager>
bool destroy_binary( node_ref<Manager> node ) noexcept
{
    return detail::destroy_backed<binary_storage<Manager>, Manager>( node, node_type::binary_value );
}

} // namespace pjson
