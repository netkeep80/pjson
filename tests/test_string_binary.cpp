#include <pjson/pjson.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

// @req FR-002 — string and binary values use their native persistent types.
// @req FR-014 — semantic type remains the outer PMM application NodeType.
int main()
{
    using StringManager = pjson::single_threaded_manager<3201>;
    if ( !StringManager::create( 8 * 1024 ) )
        return 1;

    const auto string_used   = StringManager::used_size();
    const auto string_blocks = StringManager::block_count();
    const auto* string_base  = StringManager::backend().base_ptr();

    std::vector<char> text( 32 * 1024 );
    for ( std::size_t i = 0; i < text.size(); ++i )
        text[i] = static_cast<char>( i % 251 );
    text[7] = '\0';
    text[1024] = '\0';

    auto string_node = pjson::make_string<StringManager>( text.data(), text.size() );
    if ( string_node.is_null() || !pjson::is_string<StringManager>( string_node ) ||
         StringManager::backend().base_ptr() == string_base )
        return 2;

    auto string_view = pjson::get_string<StringManager>( string_node );
    if ( !string_view.has_value() || string_view->size() != text.size() ||
         std::memcmp( string_view->data(), text.data(), text.size() ) != 0 )
        return 3;
    if ( pjson::get_binary<StringManager>( string_node ).has_value() )
        return 4;

    // Self-aliasing append grows the backing pstring. The source view points
    // into the old backing block and must be rebound internally after growth.
    if ( !pjson::append_string<StringManager>( string_node, string_view->data() + 100, 2048 ) )
        return 5;
    string_view = pjson::get_string<StringManager>( string_node );
    if ( !string_view.has_value() || string_view->size() != text.size() + 2048 ||
         std::memcmp( string_view->data() + text.size(), text.data() + 100, 2048 ) != 0 )
        return 6;

    auto string_clone = pjson::clone_string<StringManager>( string_node );
    const auto clone_view = pjson::get_string<StringManager>( string_clone );
    if ( string_clone.is_null() || !clone_view.has_value() || clone_view->size() != string_view->size() ||
         std::memcmp( clone_view->data(), string_view->data(), string_view->size() ) != 0 )
        return 7;

    const char replacement[] = { 'A', '\0', 'B', '\0', 'C' };
    if ( !pjson::set_string<StringManager>( string_clone, replacement, sizeof( replacement ) ) )
        return 8;
    const auto replaced = pjson::get_string<StringManager>( string_clone );
    if ( !replaced.has_value() || replaced->size() != sizeof( replacement ) ||
         std::memcmp( replaced->data(), replacement, sizeof( replacement ) ) != 0 )
        return 9;

    auto scalar = pjson::make_boolean<StringManager>( true );
    if ( scalar.is_null() || pjson::get_string<StringManager>( scalar ).has_value() ||
         pjson::destroy_string<StringManager>( scalar ) )
        return 10;
    if ( !pjson::destroy_scalar<StringManager>( scalar ) )
        return 11;

    if ( !pjson::destroy_string<StringManager>( string_clone ) ||
         !pjson::destroy_string<StringManager>( string_node ) )
        return 12;
    if ( StringManager::used_size() != string_used || StringManager::block_count() != string_blocks ||
         !StringManager::verify().ok )
        return 13;
    StringManager::destroy();

    using BinaryManager = pjson::single_threaded_manager<3202>;
    if ( !BinaryManager::create( 8 * 1024 ) )
        return 14;

    const auto binary_used   = BinaryManager::used_size();
    const auto binary_blocks = BinaryManager::block_count();
    const auto* binary_base  = BinaryManager::backend().base_ptr();

    std::vector<std::uint8_t> bytes( 24 * 1024 );
    for ( std::size_t i = 0; i < bytes.size(); ++i )
        bytes[i] = static_cast<std::uint8_t>( ( i * 17 ) & 0xffu );

    auto binary_node = pjson::make_binary<BinaryManager>( bytes );
    if ( binary_node.is_null() || !pjson::is_binary<BinaryManager>( binary_node ) ||
         BinaryManager::backend().base_ptr() == binary_base )
        return 15;

    auto binary_view = pjson::get_binary<BinaryManager>( binary_node );
    if ( !binary_view.has_value() || binary_view->size() != bytes.size() ||
         std::memcmp( binary_view->data(), bytes.data(), bytes.size() ) != 0 )
        return 16;
    if ( pjson::get_string<BinaryManager>( binary_node ).has_value() )
        return 17;

    // This source aliases the same PMM parray. Growth may move both the backing
    // block and the whole arena, so the source must be represented by offset.
    if ( !pjson::append_binary<BinaryManager>( binary_node, binary_view->data() + 100, 4096 ) )
        return 18;
    binary_view = pjson::get_binary<BinaryManager>( binary_node );
    if ( !binary_view.has_value() || binary_view->size() != bytes.size() + 4096 ||
         std::memcmp( binary_view->data() + bytes.size(), bytes.data() + 100, 4096 ) != 0 )
        return 19;

    // clone_binary captures an arena-backed source before allocating its new
    // native parray, then rebinds the source if that allocation relocates PMM.
    auto binary_clone = pjson::clone_binary<BinaryManager>( binary_node );
    auto clone_binary_view = pjson::get_binary<BinaryManager>( binary_clone );
    if ( binary_clone.is_null() || !clone_binary_view.has_value() ||
         clone_binary_view->size() != binary_view->size() ||
         std::memcmp( clone_binary_view->data(), binary_view->data(), binary_view->size() ) != 0 )
        return 20;

    const std::size_t clone_old_size = clone_binary_view->size();
    if ( !pjson::resize_binary<BinaryManager>( binary_clone, clone_old_size + 256 ) )
        return 21;
    clone_binary_view = pjson::get_binary<BinaryManager>( binary_clone );
    if ( !clone_binary_view.has_value() || clone_binary_view->size() != clone_old_size + 256 ||
         !std::all_of( clone_binary_view->begin() + static_cast<std::ptrdiff_t>( clone_old_size ),
                       clone_binary_view->end(), []( std::uint8_t value ) { return value == 0; } ) )
        return 22;

    auto empty_binary = pjson::make_binary<BinaryManager>( nullptr, 0 );
    auto empty_view   = pjson::get_binary<BinaryManager>( empty_binary );
    if ( empty_binary.is_null() || !empty_view.has_value() || !empty_view->empty() )
        return 23;

    // Copy from another PMM-backed value into an empty destination. The source
    // arena offset must survive destination resize and possible arena remap.
    binary_view = pjson::get_binary<BinaryManager>( binary_node );
    if ( !binary_view.has_value() ||
         !pjson::set_binary<BinaryManager>( empty_binary, binary_view->data(), binary_view->size() ) )
        return 24;
    empty_view = pjson::get_binary<BinaryManager>( empty_binary );
    binary_view = pjson::get_binary<BinaryManager>( binary_node );
    if ( !empty_view.has_value() || !binary_view.has_value() || empty_view->size() != binary_view->size() ||
         std::memcmp( empty_view->data(), binary_view->data(), binary_view->size() ) != 0 )
        return 25;

    if ( !pjson::destroy_binary<BinaryManager>( empty_binary ) ||
         !pjson::destroy_binary<BinaryManager>( binary_clone ) ||
         !pjson::destroy_binary<BinaryManager>( binary_node ) )
        return 26;
    if ( BinaryManager::used_size() != binary_used || BinaryManager::block_count() != binary_blocks ||
         !BinaryManager::verify().ok )
        return 27;

    BinaryManager::destroy();
    return 0;
}
