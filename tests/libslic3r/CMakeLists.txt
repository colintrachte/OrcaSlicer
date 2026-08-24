get_filename_component(_TEST_NAME ${CMAKE_CURRENT_LIST_DIR} NAME)

add_executable(${_TEST_NAME}_tests
    ${_TEST_NAME}_tests.cpp
    # Compiled here, not into libslic3r: ConfigRegistry has no production caller and this keeps
    # it out of the production target's PrintConfig.hpp exposure. See src/libslic3r/CMakeLists.txt.
    ${CMAKE_SOURCE_DIR}/src/libslic3r/ConfigRegistry.cpp
    test_3mf.cpp
    test_aabbindirect.cpp
    test_appconfig.cpp
    test_arachne_walls.cpp
    test_arrange.cpp
    test_bambu_networking.cpp
    test_calib.cpp
    test_clipper_offset.cpp
    test_clipper_utils.cpp
    test_config.cpp
    test_config_registry.cpp
    test_preset_bundle_loading.cpp
    test_preset_setting_id.cpp
    test_elephant_foot_compensation.cpp
    test_geometry.cpp
    test_placeholder_parser.cpp
    test_polygon.cpp
    test_mutable_polygon.cpp
    test_mutable_priority_queue.cpp
    test_stl.cpp
    test_meshboolean.cpp
    test_marchingsquares.cpp
    test_utils.cpp
    test_timeutils.cpp
    test_voronoi.cpp
    test_optimizers.cpp
    # test_png_io.cpp
    test_indexed_triangle_set.cpp
    ../libnest2d/printer_parts.cpp
    )

if (TARGET OpenVDB::openvdb)
    target_sources(${_TEST_NAME}_tests PRIVATE test_hollowing.cpp)
endif()

target_link_libraries(${_TEST_NAME}_tests test_common libslic3r Catch2::Catch2WithMain)
target_include_directories(${_TEST_NAME}_tests PRIVATE ${CMAKE_SOURCE_DIR}/src
    # libslic3r's own headers use bare #include ("PrintConfig.hpp"), not the "libslic3r/..." form
    # used elsewhere in this test tree; ConfigRegistry.cpp (compiled here, see above) needs this
    # to resolve those includes the same way it does inside the libslic3r target itself.
    ${CMAKE_SOURCE_DIR}/src/libslic3r)
set_property(TARGET ${_TEST_NAME}_tests PROPERTY FOLDER "tests")

orcaslicer_copy_test_dlls()

orcaslicer_discover_tests(${_TEST_NAME}_tests)
