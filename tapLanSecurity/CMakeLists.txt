aux_source_directory(. TapLanSecurity)
add_library(tapLanSecurity ${TapLanSecurity})
find_package(OpenSSL REQUIRED)
target_link_libraries(tapLanSecurity OpenSSL::Crypto OpenSSL::SSL)