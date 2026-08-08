#include <pjson/pjson.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

// @req FR-002 — arrays own ordered persistent child values.
// @req FR-014 — array nodes keep semantic type in PMM metadata and only a
// persistent native-container offset in the compact payload.
int main()
{
    using Manager = pjson::single_threaded_manager<3301>;
    using Ref     = pjson::node_ref<Manager>;

    if ( !Manager::create( 8 * 1024 ) )
        return 1;
    const auto baseline_used   = Manager::used_size();
    const auto baseline_blocks = Manager::block_count();
    const auto* initial_base   = Manager::backend().base_ptr();

    auto array = pjson::make_array<Manager>();
    if ( array.is_null() || !pjson::is_array<Manager>( array ) )
        return 2;
    const auto empty_size = pjson::array_size<Manager>( array );
    if ( !empty_size.has_value() || *empty_size != 0 || pjson::array_get<Manager>( array, 0 ).has_value() )
        return 3;

    auto integer = pjson::make_integer<Manager>( 11 );
    const char text[] = { 'a', '\0', 'b' };
    auto string = pjson::make_string<Manager>( text, sizeof( text ) );
    const std::uint8_t bytes[] = { 9, 8, 7, 0, 6 };
    auto binary = pjson::make_binary<Manager>( bytes, sizeof( bytes ) );
    if ( integer.is_null() || string.is_null() || binary.is_null() ||
         !pjson::array_push_back<Manager>( array, integer ) ||
         !pjson::array_push_back<Manager>( array, string ) ||
         !pjson::array_push_back<Manager>( array, binary ) )
        return 4;

    auto boolean = pjson::make_boolean<Manager>( true );
    if ( boolean.is_null() || !pjson::array_insert<Manager>( array, 1, boolean ) )
        return 5;
    auto at1 = pjson::array_get<Manager>( array, 1 );
    if ( !at1.has_value() || !pjson::get_boolean<Manager>( *at1 ).value_or( false ) )
        return 6;

    const Ref old_integer = *pjson::array_get<Manager>( array, 0 );
    auto replacement = pjson::make_unsigned_integer<Manager>( 99 );
    if ( replacement.is_null() || !pjson::array_set<Manager>( array, 0, replacement ) )
        return 7;
    if ( pjson::node_type_of<Manager>( old_integer ).has_value() ||
         pjson::get_unsigned_integer<Manager>( *pjson::array_get<Manager>( array, 0 ) ).value_or( 0 ) != 99 )
        return 8;

    const Ref old_binary = *pjson::array_get<Manager>( array, 3 );
    if ( !pjson::array_pop_back<Manager>( array ) || pjson::node_type_of<Manager>( old_binary ).has_value() )
        return 9;

    // Failed adoption leaves ownership with the caller.
    auto bounds_child = pjson::make_null<Manager>();
    if ( bounds_child.is_null() || pjson::array_insert<Manager>( array, 9999, bounds_child ) ||
         !pjson::is_null<Manager>( bounds_child ) || !pjson::destroy_scalar<Manager>( bounds_child ) )
        return 10;

    auto wrong_parent_child = pjson::make_boolean<Manager>( false );
    auto scalar_parent = pjson::make_integer<Manager>( 1 );
    if ( wrong_parent_child.is_null() || scalar_parent.is_null() ||
         pjson::array_push_back<Manager>( scalar_parent, wrong_parent_child ) ||
         !pjson::is_boolean<Manager>( wrong_parent_child ) ||
         !pjson::destroy_scalar<Manager>( wrong_parent_child ) || !pjson::destroy_scalar<Manager>( scalar_parent ) )
        return 11;

    if ( pjson::array_push_back<Manager>( array, array ) )
        return 12;

    // Object/ref ownership is deliberately not accepted until those semantic
    // phases provide executable clone/destroy implementations.
    auto unsupported = pjson::detail::allocate_node<Manager>( pjson::node_type::object_value );
    if ( unsupported.is_null() || pjson::array_push_back<Manager>( array, unsupported ) )
        return 13;
    pjson::detail::deallocate_node<Manager>( unsupported );

    // Build a deeply nested owned subtree. Every parent adopts the previous
    // child; no shared ownership or implicit reference semantics are introduced.
    auto deep = pjson::make_array<Manager>();
    if ( deep.is_null() || !pjson::array_push_back<Manager>( deep, pjson::make_string<Manager>( "leaf" ) ) )
        return 14;
    for ( int depth = 0; depth < 32; ++depth )
    {
        auto parent = pjson::make_array<Manager>();
        if ( parent.is_null() || !pjson::array_push_back<Manager>( parent, deep ) )
            return 15;
        deep = parent;
    }
    if ( !pjson::array_push_back<Manager>( array, deep ) )
        return 16;

    // Large append sequence forces both PMM arena growth and native parray
    // capacity changes. The outer array is used only through persistent identity.
    constexpr std::size_t kLargeCount = 600;
    for ( std::size_t i = 0; i < kLargeCount; ++i )
    {
        auto child = pjson::make_integer<Manager>( static_cast<std::int64_t>( i ) );
        if ( child.is_null() || !pjson::array_push_back<Manager>( array, child ) )
            return 17;
    }
    if ( Manager::backend().base_ptr() == initial_base )
        return 18;

    const auto size = pjson::array_size<Manager>( array );
    if ( !size.has_value() || *size != 4 + kLargeCount )
        return 19;
    for ( std::size_t i = 0; i < kLargeCount; ++i )
    {
        const auto child = pjson::array_get<Manager>( array, 4 + i );
        if ( !child.has_value() || pjson::get_integer<Manager>( *child ).value_or( -1 ) != static_cast<std::int64_t>( i ) )
            return 20;
    }

    auto clone = pjson::clone_array<Manager>( array );
    if ( clone.is_null() || clone == array )
        return 21;
    const auto clone_size = pjson::array_size<Manager>( clone );
    if ( !clone_size.has_value() || *clone_size != *size )
        return 22;

    const auto src0 = pjson::array_get<Manager>( array, 0 );
    const auto dst0 = pjson::array_get<Manager>( clone, 0 );
    if ( !src0.has_value() || !dst0.has_value() || *src0 == *dst0 ||
         pjson::get_unsigned_integer<Manager>( *dst0 ).value_or( 0 ) != 99 )
        return 23;

    const auto src_text = pjson::array_get<Manager>( array, 2 );
    const auto dst_text = pjson::array_get<Manager>( clone, 2 );
    const auto src_view = src_text.has_value() ? pjson::get_string<Manager>( *src_text ) : std::nullopt;
    const auto dst_view = dst_text.has_value() ? pjson::get_string<Manager>( *dst_text ) : std::nullopt;
    if ( !src_view.has_value() || !dst_view.has_value() || *src_text == *dst_text ||
         src_view->size() != sizeof( text ) || dst_view->size() != sizeof( text ) ||
         std::memcmp( src_view->data(), dst_view->data(), sizeof( text ) ) != 0 )
        return 24;

    const auto src_deep = pjson::array_get<Manager>( array, 3 );
    const auto dst_deep = pjson::array_get<Manager>( clone, 3 );
    if ( !src_deep.has_value() || !dst_deep.has_value() || *src_deep == *dst_deep ||
         !pjson::is_array<Manager>( *dst_deep ) )
        return 25;

    if ( !Manager::verify().ok )
        return 26;

    if ( !pjson::destroy_value<Manager>( clone ) || !pjson::destroy_value<Manager>( array ) )
        return 27;
    if ( Manager::used_size() != baseline_used || Manager::block_count() != baseline_blocks || !Manager::verify().ok )
        return 28;

    Manager::destroy();
    return 0;
}
