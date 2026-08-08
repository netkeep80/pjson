#include <pjson/pjson.h>

#include <array>
#include <cstdint>
#include <type_traits>

// @req FR-002 — all persistent JSON semantic types have a distinct code.
// @req FR-009 — pjson semantic codes occupy the fixed 32..41 subrange.
// @req FR-014 — node payload carries values/offsets but no type discriminator.
int main()
{
    using Manager = pjson::default_manager;
    using Payload = pjson::node_payload<Manager>;
    using Ref     = pjson::node_ref<Manager>;

    static_assert( std::is_union_v<Payload> );
    static_assert( std::is_standard_layout_v<Payload> );
    static_assert( std::is_trivially_copyable_v<Payload> );
    static_assert( sizeof( Payload ) == 8 );
    static_assert( alignof( Payload ) == 8 );
    static_assert( std::is_trivially_copyable_v<Ref> );

    static_assert( static_cast<std::uint8_t>( pjson::node_type::null_value ) == 32 );
    static_assert( static_cast<std::uint8_t>( pjson::node_type::boolean_value ) == 33 );
    static_assert( static_cast<std::uint8_t>( pjson::node_type::signed_integer ) == 34 );
    static_assert( static_cast<std::uint8_t>( pjson::node_type::unsigned_integer ) == 35 );
    static_assert( static_cast<std::uint8_t>( pjson::node_type::real_value ) == 36 );
    static_assert( static_cast<std::uint8_t>( pjson::node_type::string_value ) == 37 );
    static_assert( static_cast<std::uint8_t>( pjson::node_type::binary_value ) == 38 );
    static_assert( static_cast<std::uint8_t>( pjson::node_type::array_value ) == 39 );
    static_assert( static_cast<std::uint8_t>( pjson::node_type::object_value ) == 40 );
    static_assert( static_cast<std::uint8_t>( pjson::node_type::ref_value ) == 41 );

    if ( !pjson::is_pjson_node_type( pjson::to_pmm_node_type( pjson::node_type::null_value ) ) )
        return 1;
    if ( !pjson::is_pjson_node_type( pjson::to_pmm_node_type( pjson::node_type::ref_value ) ) )
        return 2;
    if ( pjson::is_pjson_node_type( pmm::NodeType::PPtr ) )
        return 3;
    if ( pjson::from_pmm_node_type( pmm::NodeType::Generic ).has_value() )
        return 4;

    if ( !Manager::create( 128 * 1024 ) )
        return 5;

    constexpr std::array all_types{
        pjson::node_type::null_value,
        pjson::node_type::boolean_value,
        pjson::node_type::signed_integer,
        pjson::node_type::unsigned_integer,
        pjson::node_type::real_value,
        pjson::node_type::string_value,
        pjson::node_type::binary_value,
        pjson::node_type::array_value,
        pjson::node_type::object_value,
        pjson::node_type::ref_value,
    };

    for ( const auto expected : all_types )
    {
        const auto node = pjson::detail::allocate_node<Manager>( expected );
        if ( node.is_null() )
        {
            Manager::destroy();
            return 6;
        }

        const auto actual = pjson::node_type_of<Manager>( node );
        if ( !actual.has_value() || *actual != expected )
        {
            Manager::destroy();
            return 7;
        }
        if ( Manager::get_node_type( node ) != pjson::to_pmm_node_type( expected ) )
        {
            Manager::destroy();
            return 8;
        }

        const auto* payload = Manager::resolve( node );
        if ( payload == nullptr || payload->unsigned_value != 0 )
        {
            Manager::destroy();
            return 9;
        }

        pjson::detail::deallocate_node<Manager>( node );
    }

    // Reserved pjson codes are not valid semantic node types yet.
    const auto reserved = static_cast<pjson::node_type>( 42 );
    if ( !pjson::detail::allocate_node<Manager>( reserved ).is_null() )
    {
        Manager::destroy();
        return 10;
    }

    // A plain PMM Generic block is not accidentally interpreted as a pjson node.
    const auto generic = Manager::allocate_typed<Payload>();
    if ( generic.is_null() || pjson::node_type_of<Manager>( generic ).has_value() )
    {
        Manager::destroy();
        return 11;
    }
    Manager::deallocate_typed( generic );

    Ref null_ref;
    if ( pjson::node_type_of<Manager>( null_ref ).has_value() )
    {
        Manager::destroy();
        return 12;
    }

    if ( !Manager::verify().ok )
    {
        Manager::destroy();
        return 13;
    }

    Manager::destroy();
    return 0;
}
