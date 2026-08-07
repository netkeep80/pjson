#pragma once

#include <cstdint>
#include <optional>
#include <type_traits>

#include "pjson/manager.h"

namespace pjson
{

// @req FR-002 — pjson exposes the complete semantic JSON value set.
// @req FR-009 — semantic types occupy pjson's fixed application NodeType subrange.
enum class node_type : std::uint8_t
{
    null_value       = 32,
    boolean_value    = 33,
    signed_integer   = 34,
    unsigned_integer = 35,
    real_value       = 36,
    string_value     = 37,
    binary_value     = 38,
    array_value      = 39,
    object_value     = 40,
    ref_value        = 41,
};

inline constexpr std::uint8_t pjson_node_type_first = static_cast<std::uint8_t>( node_type::null_value );
inline constexpr std::uint8_t pjson_node_type_last  = static_cast<std::uint8_t>( node_type::ref_value );
inline constexpr std::uint8_t pjson_node_type_reserved_last = 63;

constexpr pmm::NodeType to_pmm_node_type( node_type type ) noexcept
{
    return static_cast<pmm::NodeType>( static_cast<std::uint8_t>( type ) );
}

constexpr bool is_pjson_node_type( pmm::NodeType type ) noexcept
{
    const auto raw = static_cast<std::uint8_t>( type );
    return raw >= pjson_node_type_first && raw <= pjson_node_type_last;
}

constexpr std::optional<node_type> from_pmm_node_type( pmm::NodeType type ) noexcept
{
    if ( !is_pjson_node_type( type ) )
        return std::nullopt;
    return static_cast<node_type>( static_cast<std::uint8_t>( type ) );
}

// @req FR-014 — semantic type is stored only in PMM block metadata; the
// persistent payload therefore contains no node_tag/discriminator.
//
// Container-backed values store only a persistent PMM granule offset. A view
// reconstructs the appropriate typed pptr after inspecting the semantic
// NodeType. Keeping the payload independent of container C++ types also avoids
// recursive/cyclic type definitions for arrays and objects.
template <typename Manager> union alignas( 8 ) node_payload
{
    std::uint8_t                 boolean_value;
    std::int64_t                 signed_value;
    std::uint64_t                unsigned_value;
    double                       real_value;
    typename Manager::index_type storage_offset;
};

template <typename Manager>
using node_ref = typename Manager::template pptr<node_payload<Manager>>;

template <typename Manager>
std::optional<node_type> node_type_of( node_ref<Manager> node ) noexcept
{
    if ( node.is_null() )
        return std::nullopt;
    return from_pmm_node_type( Manager::get_node_type( node ) );
}

namespace detail
{

// Low-level node allocation only. Type-specific layers own any backing
// pstring/parray/pmap object and must destroy that backing storage before this
// payload is deallocated.
template <typename Manager>
node_ref<Manager> allocate_node( node_type type ) noexcept
{
    const pmm::NodeType pmm_type = to_pmm_node_type( type );
    if ( !is_pjson_node_type( pmm_type ) )
        return {};

    auto node = Manager::template allocate_typed<node_payload<Manager>>();
    if ( node.is_null() )
        return {};

    auto* payload = Manager::resolve( node );
    if ( payload == nullptr )
    {
        Manager::deallocate_typed( node );
        return {};
    }

    // Deterministic persistent bytes for null and for not-yet-initialized
    // scalar/backing values. This also starts the uint64 union member lifetime.
    payload->unsigned_value = 0;

    if ( !Manager::set_application_node_type( node, pmm_type ) )
    {
        Manager::deallocate_typed( node );
        return {};
    }
    return node;
}

template <typename Manager>
void deallocate_node( node_ref<Manager> node ) noexcept
{
    Manager::deallocate_typed( node );
}

} // namespace detail

static_assert( pjson_node_type_first == 32 );
static_assert( pjson_node_type_reserved_last < 256 );
static_assert( pjson_node_type_first > static_cast<std::uint8_t>( pmm::NodeType::PPtr ) );
static_assert( sizeof( node_payload<default_manager> ) == sizeof( std::uint64_t ) );
static_assert( alignof( node_payload<default_manager> ) == 8 );
static_assert( std::is_standard_layout_v<node_payload<default_manager>> );
static_assert( std::is_trivially_copyable_v<node_payload<default_manager>> );
static_assert( std::is_trivially_copyable_v<node_ref<default_manager>> );

} // namespace pjson
