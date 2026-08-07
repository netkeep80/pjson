#pragma once

#include <cstdint>
#include <cstring>
#include <optional>

#include "pjson/node.h"

namespace pjson
{

// @req FR-002 — null, boolean, signed/unsigned integers and real are the
// scalar semantic types of the pjson value model.
constexpr bool is_scalar_type( node_type type ) noexcept
{
    const auto raw = static_cast<std::uint8_t>( type );
    return raw >= static_cast<std::uint8_t>( node_type::null_value ) &&
           raw <= static_cast<std::uint8_t>( node_type::real_value );
}

template <typename Manager>
bool has_type( node_ref<Manager> node, node_type expected ) noexcept
{
    const auto actual = node_type_of<Manager>( node );
    return actual.has_value() && *actual == expected;
}

template <typename Manager> bool is_null( node_ref<Manager> node ) noexcept
{
    return has_type<Manager>( node, node_type::null_value );
}
template <typename Manager> bool is_boolean( node_ref<Manager> node ) noexcept
{
    return has_type<Manager>( node, node_type::boolean_value );
}
template <typename Manager> bool is_integer( node_ref<Manager> node ) noexcept
{
    return has_type<Manager>( node, node_type::signed_integer );
}
template <typename Manager> bool is_unsigned_integer( node_ref<Manager> node ) noexcept
{
    return has_type<Manager>( node, node_type::unsigned_integer );
}
template <typename Manager> bool is_real( node_ref<Manager> node ) noexcept
{
    return has_type<Manager>( node, node_type::real_value );
}

namespace detail
{

template <typename Manager, typename Writer>
node_ref<Manager> make_scalar( node_type type, Writer&& writer ) noexcept
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
    writer( *payload );
    return node;
}

template <typename Manager, typename Writer>
bool mutate_scalar( node_ref<Manager> node, node_type target, Writer&& writer ) noexcept
{
    const auto current = node_type_of<Manager>( node );
    if ( !current.has_value() || !is_scalar_type( *current ) || !is_scalar_type( target ) )
        return false;

    auto* payload = Manager::resolve( node );
    if ( payload == nullptr )
        return false;

    // Issue #51 tracks a future scoped PMM access primitive for concurrent
    // relocation safety. Until then this operation requires single-threaded or
    // externally serialized access to the selected Manager instance.
    writer( *payload );
    return Manager::set_application_node_type( node, to_pmm_node_type( target ) );
}

template <typename Manager, typename Value, typename Reader>
std::optional<Value> read_scalar( node_ref<Manager> node, node_type expected, Reader&& reader ) noexcept
{
    if ( !has_type<Manager>( node, expected ) )
        return std::nullopt;
    const auto* payload = Manager::resolve( node );
    if ( payload == nullptr )
        return std::nullopt;
    return reader( *payload );
}

} // namespace detail

template <typename Manager>
node_ref<Manager> make_null() noexcept
{
    return detail::make_scalar<Manager>( node_type::null_value,
                                         []( auto& p ) noexcept { p.unsigned_value = 0; } );
}

template <typename Manager>
node_ref<Manager> make_boolean( bool value ) noexcept
{
    return detail::make_scalar<Manager>( node_type::boolean_value,
                                         [value]( auto& p ) noexcept { p.boolean_value = value ? 1u : 0u; } );
}

template <typename Manager>
node_ref<Manager> make_integer( std::int64_t value ) noexcept
{
    return detail::make_scalar<Manager>( node_type::signed_integer,
                                         [value]( auto& p ) noexcept { p.signed_value = value; } );
}

template <typename Manager>
node_ref<Manager> make_unsigned_integer( std::uint64_t value ) noexcept
{
    return detail::make_scalar<Manager>( node_type::unsigned_integer,
                                         [value]( auto& p ) noexcept { p.unsigned_value = value; } );
}

template <typename Manager>
node_ref<Manager> make_real( double value ) noexcept
{
    // The persistent scalar layer preserves every IEEE-754 double bit pattern,
    // including NaN and infinities. Strict JSON codec policy is applied later
    // by the serializer/parser layer rather than silently changing stored data.
    return detail::make_scalar<Manager>( node_type::real_value,
                                         [value]( auto& p ) noexcept { p.real_value = value; } );
}

template <typename Manager>
std::optional<bool> get_boolean( node_ref<Manager> node ) noexcept
{
    return detail::read_scalar<Manager, bool>( node, node_type::boolean_value,
                                               []( const auto& p ) noexcept { return p.boolean_value != 0; } );
}

template <typename Manager>
std::optional<std::int64_t> get_integer( node_ref<Manager> node ) noexcept
{
    return detail::read_scalar<Manager, std::int64_t>( node, node_type::signed_integer,
                                                       []( const auto& p ) noexcept { return p.signed_value; } );
}

template <typename Manager>
std::optional<std::uint64_t> get_unsigned_integer( node_ref<Manager> node ) noexcept
{
    return detail::read_scalar<Manager, std::uint64_t>( node, node_type::unsigned_integer,
                                                        []( const auto& p ) noexcept { return p.unsigned_value; } );
}

template <typename Manager>
std::optional<double> get_real( node_ref<Manager> node ) noexcept
{
    return detail::read_scalar<Manager, double>( node, node_type::real_value,
                                                 []( const auto& p ) noexcept { return p.real_value; } );
}

template <typename Manager>
bool set_null( node_ref<Manager> node ) noexcept
{
    return detail::mutate_scalar<Manager>( node, node_type::null_value,
                                           []( auto& p ) noexcept { p.unsigned_value = 0; } );
}

template <typename Manager>
bool set_boolean( node_ref<Manager> node, bool value ) noexcept
{
    return detail::mutate_scalar<Manager>( node, node_type::boolean_value,
                                           [value]( auto& p ) noexcept { p.boolean_value = value ? 1u : 0u; } );
}

template <typename Manager>
bool set_integer( node_ref<Manager> node, std::int64_t value ) noexcept
{
    return detail::mutate_scalar<Manager>( node, node_type::signed_integer,
                                           [value]( auto& p ) noexcept { p.signed_value = value; } );
}

template <typename Manager>
bool set_unsigned_integer( node_ref<Manager> node, std::uint64_t value ) noexcept
{
    return detail::mutate_scalar<Manager>( node, node_type::unsigned_integer,
                                           [value]( auto& p ) noexcept { p.unsigned_value = value; } );
}

template <typename Manager>
bool set_real( node_ref<Manager> node, double value ) noexcept
{
    return detail::mutate_scalar<Manager>( node, node_type::real_value,
                                           [value]( auto& p ) noexcept { p.real_value = value; } );
}

template <typename Manager>
node_ref<Manager> clone_scalar( node_ref<Manager> source ) noexcept
{
    const auto type = node_type_of<Manager>( source );
    if ( !type.has_value() || !is_scalar_type( *type ) )
        return {};

    const auto* src = Manager::resolve( source );
    if ( src == nullptr )
        return {};

    auto clone = detail::allocate_node<Manager>( *type );
    if ( clone.is_null() )
        return {};
    auto* dst = Manager::resolve( clone );
    if ( dst == nullptr )
    {
        detail::deallocate_node<Manager>( clone );
        return {};
    }

    static_assert( std::is_trivially_copyable_v<node_payload<Manager>> );
    std::memcpy( dst, src, sizeof( node_payload<Manager> ) );
    return clone;
}

template <typename Manager>
bool destroy_scalar( node_ref<Manager> node ) noexcept
{
    const auto type = node_type_of<Manager>( node );
    if ( !type.has_value() || !is_scalar_type( *type ) )
        return false;
    detail::deallocate_node<Manager>( node );
    return true;
}

} // namespace pjson
