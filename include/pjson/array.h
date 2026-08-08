#pragma once

#include <cstddef>
#include <optional>

#include "pjson/scalar.h"
#include "pjson/string_binary.h"
#include "pmm/parray.h"

namespace pjson
{

// @req FR-002 — JSON arrays are first-class persistent pjson values.
// @req FR-014 — the outer semantic type remains PMM NodeType metadata and the
// compact pjson payload stores only the native PMM backing-object offset.
template <typename Manager> using array_storage = pmm::parray<node_ref<Manager>, Manager>;

template <typename Manager> bool is_array( node_ref<Manager> node ) noexcept
{
    return has_type<Manager>( node, node_type::array_value );
}

template <typename Manager> node_ref<Manager> clone_value( node_ref<Manager> source ) noexcept;
template <typename Manager> bool destroy_value( node_ref<Manager> node ) noexcept;

namespace detail
{

template <typename Manager>
bool is_owned_value( node_ref<Manager> node ) noexcept
{
    const auto type = node_type_of<Manager>( node );
    if ( !type.has_value() )
        return false;
    return is_scalar_type( *type ) || *type == node_type::string_value || *type == node_type::binary_value ||
           *type == node_type::array_value;
}

template <typename Manager>
pmm::pptr<array_storage<Manager>, Manager> array_backing( node_ref<Manager> node ) noexcept
{
    return backing_ref<array_storage<Manager>, Manager>( node, node_type::array_value );
}

} // namespace detail

template <typename Manager>
node_ref<Manager> make_array() noexcept
{
    using Storage = array_storage<Manager>;
    auto backing = Manager::template create_typed<Storage>();
    if ( backing.is_null() )
        return {};

    auto node = detail::bind_backing<Storage, Manager>( node_type::array_value, backing );
    if ( node.is_null() )
        detail::cleanup_backing<Storage, Manager>( backing );
    return node;
}

template <typename Manager>
std::optional<std::size_t> array_size( node_ref<Manager> array ) noexcept
{
    const auto backing = detail::array_backing<Manager>( array );
    const auto* state = Manager::resolve( backing );
    return state ? std::optional<std::size_t>( state->size() ) : std::nullopt;
}

template <typename Manager>
std::optional<node_ref<Manager>> array_get( node_ref<Manager> array, std::size_t index ) noexcept
{
    const auto backing = detail::array_backing<Manager>( array );
    const auto* state = Manager::resolve( backing );
    if ( state == nullptr )
        return std::nullopt;
    const auto* value = state->at( index );
    return value ? std::optional<node_ref<Manager>>( *value ) : std::nullopt;
}

// Ownership contract: a successful insertion adopts `child`. The caller must
// not give the same owned node to another parent; shared/cyclic identity is a
// later explicit `ref` semantic rather than implicit container aliasing.
template <typename Manager>
bool array_push_back( node_ref<Manager> array, node_ref<Manager> child ) noexcept
{
    if ( child.is_null() || child == array || !detail::is_owned_value<Manager>( child ) )
        return false;
    const auto backing = detail::array_backing<Manager>( array );
    auto* state = Manager::resolve( backing );
    return state != nullptr && state->push_back( child );
}

template <typename Manager>
bool array_insert( node_ref<Manager> array, std::size_t index, node_ref<Manager> child ) noexcept
{
    if ( child.is_null() || child == array || !detail::is_owned_value<Manager>( child ) )
        return false;
    const auto backing = detail::array_backing<Manager>( array );
    auto* state = Manager::resolve( backing );
    return state != nullptr && state->insert( index, child );
}

template <typename Manager>
bool array_set( node_ref<Manager> array, std::size_t index, node_ref<Manager> child ) noexcept
{
    if ( child.is_null() || child == array || !detail::is_owned_value<Manager>( child ) )
        return false;

    const auto backing = detail::array_backing<Manager>( array );
    auto* state = Manager::resolve( backing );
    if ( state == nullptr )
        return false;
    const auto* old_ptr = state->at( index );
    if ( old_ptr == nullptr )
        return false;
    const node_ref<Manager> old = *old_ptr;
    if ( old == child )
        return true;
    if ( !detail::is_owned_value<Manager>( old ) || !state->set( index, child ) )
        return false;
    return destroy_value<Manager>( old );
}

template <typename Manager>
bool array_pop_back( node_ref<Manager> array ) noexcept
{
    const auto backing = detail::array_backing<Manager>( array );
    auto* state = Manager::resolve( backing );
    if ( state == nullptr || state->empty() )
        return false;
    const auto* last = state->back();
    if ( last == nullptr || !detail::is_owned_value<Manager>( *last ) )
        return false;
    const node_ref<Manager> child = *last;
    if ( !destroy_value<Manager>( child ) )
        return false;
    state = Manager::resolve( backing );
    if ( state == nullptr || state->empty() )
        return false;
    state->pop_back();
    return true;
}

template <typename Manager>
bool array_erase( node_ref<Manager> array, std::size_t index ) noexcept
{
    const auto backing = detail::array_backing<Manager>( array );
    auto* state = Manager::resolve( backing );
    if ( state == nullptr )
        return false;
    const auto* child_ptr = state->at( index );
    if ( child_ptr == nullptr || !detail::is_owned_value<Manager>( *child_ptr ) )
        return false;
    const node_ref<Manager> child = *child_ptr;
    if ( !destroy_value<Manager>( child ) )
        return false;
    state = Manager::resolve( backing );
    return state != nullptr && state->erase( index );
}

template <typename Manager>
bool array_clear( node_ref<Manager> array ) noexcept
{
    const auto size = array_size<Manager>( array );
    if ( !size.has_value() )
        return false;
    for ( std::size_t remaining = *size; remaining > 0; --remaining )
    {
        if ( !array_pop_back<Manager>( array ) )
            return false;
    }
    return true;
}

template <typename Manager>
node_ref<Manager> clone_array( node_ref<Manager> source ) noexcept
{
    const auto count = array_size<Manager>( source );
    if ( !count.has_value() )
        return {};

    auto clone = make_array<Manager>();
    if ( clone.is_null() )
        return {};

    for ( std::size_t i = 0; i < *count; ++i )
    {
        // Every child is fetched again through the persistent source backing:
        // cloning the preceding child may have expanded/remapped the arena.
        const auto child = array_get<Manager>( source, i );
        if ( !child.has_value() )
        {
            destroy_value<Manager>( clone );
            return {};
        }
        auto child_clone = clone_value<Manager>( *child );
        if ( child_clone.is_null() )
        {
            destroy_value<Manager>( clone );
            return {};
        }
        if ( !array_push_back<Manager>( clone, child_clone ) )
        {
            destroy_value<Manager>( child_clone );
            destroy_value<Manager>( clone );
            return {};
        }
    }
    return clone;
}

template <typename Manager>
bool destroy_array( node_ref<Manager> array ) noexcept
{
    using Storage = array_storage<Manager>;
    const auto backing = detail::array_backing<Manager>( array );
    if ( backing.is_null() || !array_clear<Manager>( array ) )
        return false;
    detail::cleanup_backing<Storage, Manager>( backing );
    detail::deallocate_node<Manager>( array );
    return true;
}

// Common value lifecycle dispatch deliberately covers only semantic types whose
// ownership implementation is executable today. Object/ref cases are added by
// their own phases instead of accepting nodes that cannot yet be freed safely.
template <typename Manager>
node_ref<Manager> clone_value( node_ref<Manager> source ) noexcept
{
    const auto type = node_type_of<Manager>( source );
    if ( !type.has_value() )
        return {};
    if ( is_scalar_type( *type ) )
        return clone_scalar<Manager>( source );
    switch ( *type )
    {
    case node_type::string_value:
        return clone_string<Manager>( source );
    case node_type::binary_value:
        return clone_binary<Manager>( source );
    case node_type::array_value:
        return clone_array<Manager>( source );
    default:
        return {};
    }
}

template <typename Manager>
bool destroy_value( node_ref<Manager> node ) noexcept
{
    const auto type = node_type_of<Manager>( node );
    if ( !type.has_value() )
        return false;
    if ( is_scalar_type( *type ) )
        return destroy_scalar<Manager>( node );
    switch ( *type )
    {
    case node_type::string_value:
        return destroy_string<Manager>( node );
    case node_type::binary_value:
        return destroy_binary<Manager>( node );
    case node_type::array_value:
        return destroy_array<Manager>( node );
    default:
        return false;
    }
}

} // namespace pjson
