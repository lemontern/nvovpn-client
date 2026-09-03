cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

project(NvoVPN_Tests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

enable_testing()

find_package(Qt6 REQUIRED COMPONENTS Core Network Test Core5Compat)

set(CLIENT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../client)

# ───────────────────────────────── test_utilities ─────────────────────────────────
add_executable(test_utilities
    test_utilities.cpp
    ${CLIENT_DIR}/core/utils/utilities.cpp
)
target_include_directories(test_utilities PRIVATE
    ${CLIENT_DIR}
    ${CLIENT_DIR}/3rd
)
target_link_libraries(test_utilities PRIVATE Qt6::Core Qt6::Network Qt6::Test Qt6::Core5Compat)
add_test(NAME test_utilities COMMAND test_utilities)

# ───────────────────────────────── test_network_utilities ─────────────────────────
add_executable(test_network_utilities
    test_network_utilities.cpp
    ${CLIENT_DIR}/core/utils/networkUtilities.cpp
)
target_include_directories(test_network_utilities PRIVATE
    ${CLIENT_DIR}
    ${CLIENT_DIR}/3rd
)
target_link_libraries(test_network_utilities PRIVATE Qt6::Core Qt6::Network Qt6::Test Qt6::Core5Compat)
add_test(NAME test_network_utilities COMMAND test_network_utilities)

# ───────────────────────────────── test_ipaddress ─────────────────────────────────
add_executable(test_ipaddress
    test_ipaddress.cpp
    ${CLIENT_DIR}/mozilla/shared/ipaddress.cpp
)
target_include_directories(test_ipaddress PRIVATE
    ${CLIENT_DIR}/mozilla/shared
)
target_link_libraries(test_ipaddress PRIVATE Qt6::Core Qt6::Network Qt6::Test)
add_test(NAME test_ipaddress COMMAND test_ipaddress)

# ───────────────────────────────── test_serialization ─────────────────────────────
add_executable(test_serialization
    test_serialization.cpp
    ${CLIENT_DIR}/core/utils/serialization/ss.cpp
    ${CLIENT_DIR}/core/utils/serialization/trojan.cpp
    ${CLIENT_DIR}/core/utils/serialization/vless.cpp
    ${CLIENT_DIR}/core/utils/serialization/vmess.cpp
    ${CLIENT_DIR}/core/utils/serialization/vmess_new.cpp
    ${CLIENT_DIR}/core/utils/serialization/outbound.cpp
    ${CLIENT_DIR}/core/utils/serialization/inbound.cpp
    ${CLIENT_DIR}/core/utils/utilities.cpp
)
target_include_directories(test_serialization PRIVATE
    ${CLIENT_DIR}
    ${CLIENT_DIR}/3rd
)
target_link_libraries(test_serialization PRIVATE Qt6::Core Qt6::Network Qt6::Test Qt6::Core5Compat)
add_test(NAME test_serialization COMMAND test_serialization)
