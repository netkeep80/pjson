#include <pjson/pjson.h>

#include <cmath>
#include <cstdint>
#include <limits>

// @req FR-002 — scalar values preserve their declared JSON semantics.
// @req FR-014 — scalar runtime type comes from PMM NodeType metadata.
int main()
{
    using Manager = pjson::default_manager;

    static_assert( pjson::is_scalar_type( pjson::node_type::null_value ) );
    static_assert( pjson::is_scalar_type( pjson::node_type::real_value ) );
    static_assert( !pjson::is_scalar_type( pjson::node_type::string_value ) );
    static_assert( !pjson::is_scalar_type( static_cast<pjson::node_type>( 0 ) ) );

    if ( !Manager::create( 128 * 1024 ) )
        return 1;

    const auto baseline_used   = Manager::used_size();
    const auto baseline_blocks = Manager::block_count();

    auto null_node = pjson::make_null<Manager>();
    if ( null_node.is_null() || !pjson::is_null<Manager>( null_node ) )
        return 2;
    if ( pjson::get_boolean<Manager>( null_node ).has_value() ||
         pjson::get_integer<Manager>( null_node ).has_value() ||
         pjson::get_unsigned_integer<Manager>( null_node ).has_value() ||
         pjson::get_real<Manager>( null_node ).has_value() )
        return 3;

    auto boolean_node = pjson::make_boolean<Manager>( true );
    if ( boolean_node.is_null() || !pjson::is_boolean<Manager>( boolean_node ) ||
         pjson::get_boolean<Manager>( boolean_node ) != true )
        return 4;

    auto integer_node = pjson::make_integer<Manager>( ( std::numeric_limits<std::int64_t>::min )() );
    if ( integer_node.is_null() || !pjson::is_integer<Manager>( integer_node ) ||
         pjson::get_integer<Manager>( integer_node ) != ( std::numeric_limits<std::int64_t>::min )() )
        return 5;
    if ( !pjson::set_integer<Manager>( integer_node, ( std::numeric_limits<std::int64_t>::max )() ) ||
         pjson::get_integer<Manager>( integer_node ) != ( std::numeric_limits<std::int64_t>::max )() )
        return 6;

    auto unsigned_node = pjson::make_unsigned_integer<Manager>( ( std::numeric_limits<std::uint64_t>::max )() );
    if ( unsigned_node.is_null() || !pjson::is_unsigned_integer<Manager>( unsigned_node ) ||
         pjson::get_unsigned_integer<Manager>( unsigned_node ) != ( std::numeric_limits<std::uint64_t>::max )() )
        return 7;

    auto negative_zero = pjson::make_real<Manager>( -0.0 );
    const auto negative_zero_value = pjson::get_real<Manager>( negative_zero );
    if ( negative_zero.is_null() || !negative_zero_value.has_value() || *negative_zero_value != 0.0 ||
         !std::signbit( *negative_zero_value ) )
        return 8;

    auto positive_zero = pjson::make_real<Manager>( 0.0 );
    const auto positive_zero_value = pjson::get_real<Manager>( positive_zero );
    if ( positive_zero.is_null() || !positive_zero_value.has_value() || *positive_zero_value != 0.0 ||
         std::signbit( *positive_zero_value ) )
        return 9;

    auto positive_inf = pjson::make_real<Manager>( ( std::numeric_limits<double>::infinity )() );
    auto negative_inf = pjson::make_real<Manager>( -( std::numeric_limits<double>::infinity )() );
    auto nan_node     = pjson::make_real<Manager>( ( std::numeric_limits<double>::quiet_NaN )() );
    const auto positive_inf_value = pjson::get_real<Manager>( positive_inf );
    const auto negative_inf_value = pjson::get_real<Manager>( negative_inf );
    const auto nan_value          = pjson::get_real<Manager>( nan_node );
    if ( !positive_inf_value.has_value() || !std::isinf( *positive_inf_value ) || *positive_inf_value < 0.0 )
        return 10;
    if ( !negative_inf_value.has_value() || !std::isinf( *negative_inf_value ) || *negative_inf_value > 0.0 )
        return 11;
    if ( !nan_value.has_value() || !std::isnan( *nan_value ) )
        return 12;

    // Type transitions reuse the same persistent node while updating the PMM
    // semantic NodeType. Old typed getters become deterministic nullopt.
    if ( !pjson::set_real<Manager>( integer_node, -0.0 ) ||
         pjson::get_integer<Manager>( integer_node ).has_value() )
        return 13;
    const auto transitioned_real = pjson::get_real<Manager>( integer_node );
    if ( !transitioned_real.has_value() || !std::signbit( *transitioned_real ) )
        return 14;
    if ( !pjson::set_boolean<Manager>( integer_node, false ) ||
         pjson::get_boolean<Manager>( integer_node ) != false )
        return 15;
    if ( !pjson::set_unsigned_integer<Manager>( integer_node, ( std::numeric_limits<std::uint64_t>::max )() ) ||
         pjson::get_unsigned_integer<Manager>( integer_node ) != ( std::numeric_limits<std::uint64_t>::max )() )
        return 16;
    if ( !pjson::set_null<Manager>( integer_node ) || !pjson::is_null<Manager>( integer_node ) )
        return 17;

    auto cloned_unsigned = pjson::clone_scalar<Manager>( unsigned_node );
    if ( cloned_unsigned.is_null() ||
         pjson::get_unsigned_integer<Manager>( cloned_unsigned ) != ( std::numeric_limits<std::uint64_t>::max )() )
        return 18;

    auto cloned_negative_zero = pjson::clone_scalar<Manager>( negative_zero );
    const auto cloned_negative_zero_value = pjson::get_real<Manager>( cloned_negative_zero );
    if ( cloned_negative_zero.is_null() || !cloned_negative_zero_value.has_value() ||
         !std::signbit( *cloned_negative_zero_value ) )
        return 19;

    auto cloned_nan = pjson::clone_scalar<Manager>( nan_node );
    const auto cloned_nan_value = pjson::get_real<Manager>( cloned_nan );
    if ( cloned_nan.is_null() || !cloned_nan_value.has_value() || !std::isnan( *cloned_nan_value ) )
        return 20;

    // Scalar helpers reject container semantic nodes and never free their future
    // backing storage by mistake.
    auto string_stub = pjson::detail::allocate_node<Manager>( pjson::node_type::string_value );
    if ( string_stub.is_null() || !pjson::clone_scalar<Manager>( string_stub ).is_null() ||
         pjson::destroy_scalar<Manager>( string_stub ) )
        return 21;
    if ( pjson::set_boolean<Manager>( string_stub, true ) || pjson::get_boolean<Manager>( string_stub ).has_value() )
        return 22;
    pjson::detail::deallocate_node<Manager>( string_stub );

    if ( !pjson::destroy_scalar<Manager>( null_node ) ||
         !pjson::destroy_scalar<Manager>( boolean_node ) ||
         !pjson::destroy_scalar<Manager>( integer_node ) ||
         !pjson::destroy_scalar<Manager>( unsigned_node ) ||
         !pjson::destroy_scalar<Manager>( negative_zero ) ||
         !pjson::destroy_scalar<Manager>( positive_zero ) ||
         !pjson::destroy_scalar<Manager>( positive_inf ) ||
         !pjson::destroy_scalar<Manager>( negative_inf ) ||
         !pjson::destroy_scalar<Manager>( nan_node ) ||
         !pjson::destroy_scalar<Manager>( cloned_unsigned ) ||
         !pjson::destroy_scalar<Manager>( cloned_negative_zero ) ||
         !pjson::destroy_scalar<Manager>( cloned_nan ) )
        return 23;

    if ( Manager::used_size() != baseline_used || Manager::block_count() != baseline_blocks )
        return 24;
    if ( !Manager::verify().ok )
        return 25;

    Manager::destroy();
    return 0;
}
