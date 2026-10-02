get_filename_component(_TEST_NAME ${CMAKE_CURRENT_LIST_DIR} NAME)

add_executable(${_TEST_NAME}_tests
    ${_TEST_NAME}_tests.cpp
    # Compiled here, not into libslic3r: ConfigRegistry has no production caller and this keeps
    # it out of the production target's PrintConfig.hpp exposure. See src/libslic3r/CMakeLists.txt.
    ${CMAKE_SOURCE_DIR}/src/libslic3r/ConfigRegistry.cpp
    test_3mf.cpp
    # Round-trip seam metadata and active/dormant volume settings in both formats.
    test_precise_seam_3mf.cpp
    test_aabbindirect.cpp
    test_appconfig.cpp
    test_arachne_walls.cpp
    test_arrange.cpp
    test_bambu_networking.cpp
    test_buildvolume.cpp
    test_calib.cpp
    test_clipper_offset.cpp
    test_clipper_utils.cpp
    test_config.cpp
    test_config_registry.cpp
    test_config_variant_expansion.cpp
    test_diagnostics.cpp
    test_locales_utils.cpp
    test_drc.cpp
    test_toolordering_nozzle_group.cpp
    test_parallel_resolve.cpp
    test_preset_bundle_loading.cpp
    test_preset_setting_id.cpp
    test_preset_diff.cpp
    test_vendor_cache.cpp
    test_preset_options.cpp
    test_elephant_foot_compensation.cpp
    test_fill_corner_smoothing.cpp
    test_filament_mixer.cpp
    test_fill_plane_path.cpp
    test_geometry.cpp
    test_multimaterial_segmentation.cpp
    test_placeholder_parser.cpp
    test_polygon.cpp
    test_mutable_polygon.cpp
    test_mutable_priority_queue.cpp
    test_minimum_spanning_tree.cpp
    test_nozzle_volume_type.cpp
    test_obj.cpp
    test_step.cpp
    test_stl.cpp
    test_triangle_selector.cpp
    test_meshboolean.cpp
    test_trianglemesh_slicer.cpp
    test_marchingsquares.cpp
    test_lay_on_face.cpp
    test_model.cpp
    test_utils.cpp
    test_miniz_extension.cpp
    test_timeutils.cpp
    test_voronoi.cpp
    test_wipe_tower_estimate.cpp
    test_wipe_tower.cpp
    test_wipe_path.cpp
    test_optimizers.cpp
    test_ordering_strategies.cpp
    # test_png_io.cpp
    test_indexed_triangle_set.cpp
    test_texture_displacement.cpp
    test_instance_lock.cpp
    ../libnest2d/printer_parts.cpp
    )

if (SLIC3R_CAD)
    target_sources(${_TEST_NAME}_tests PRIVATE
        test_caddocument.cpp
        test_sketchconstraints.cpp
        test_sketchedit.cpp
        test_sketchprofile.cpp
        test_sketchimport.cpp
        test_sketchinference.cpp
        test_slvs_constraints.cpp)
endif ()

add_executable(slic3r_config_tests
    test_config_core.cpp
)

target_link_libraries(slic3r_config_tests
    test_common
    slic3r_config
    Catch2::Catch2WithMain
)
target_include_directories(slic3r_config_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/libslic3r)
set_property(TARGET slic3r_config_tests PROPERTY FOLDER "tests")

orcaslicer_discover_tests(slic3r_config_tests)

if (TARGET OpenVDB::openvdb)
    target_sources(${_TEST_NAME}_tests PRIVATE test_hollowing.cpp)
endif()

# libslic3r links Draco privately; test_drc.cpp encodes its own fixtures.
find_package(Draco REQUIRED)
target_link_libraries(${_TEST_NAME}_tests test_common libslic3r draco::draco Catch2::Catch2WithMain)
target_include_directories(${_TEST_NAME}_tests PRIVATE ${CMAKE_SOURCE_DIR}/src
    # libslic3r's own headers use bare #include ("PrintConfig.hpp"), not the "libslic3r/..." form
    # used elsewhere in this test tree; ConfigRegistry.cpp (compiled here, see above) needs this
    # to resolve those includes the same way it does inside the libslic3r target itself.
    ${CMAKE_SOURCE_DIR}/src/libslic3r)
set_property(TARGET ${_TEST_NAME}_tests PROPERTY FOLDER "tests")

orcaslicer_copy_test_dlls()

orcaslicer_discover_tests(${_TEST_NAME}_tests)
