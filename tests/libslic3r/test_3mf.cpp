
#include "libslic3r/Model.hpp"
#include "libslic3r/Format/3mf.hpp"
#include "libslic3r/Format/bbs_3mf.hpp"
#include "libslic3r/Format/STL.hpp"
#include "libslic3r/Preset.hpp"
#include "libslic3r/Zipper.hpp"
#include "libslic3r/miniz_extension.hpp"

#include <boost/filesystem/operations.hpp>

#include <catch2/catch_tostring.hpp>
#include <Eigen/Core>
#include <Eigen/Geometry>
#include <fstream>
#include <type_traits> // for std::enable_if_t
#include <typeinfo>    // for typeid

namespace Catch {
    template <typename T>
    struct is_eigen_matrix : std::is_base_of<Eigen::MatrixBase<T>, T> {};

    template <typename T>
    struct StringMaker<T, std::enable_if_t<is_eigen_matrix<T>::value>> {
        static std::string convert(const T& eigen_obj) {
            // Newline at end of rows
            Eigen::IOFormat fmt(4, 0, ", ", "\n", "[", "]");
            std::stringstream ss;
            ss << "Matrix<" << typeid(eigen_obj).name() << "> = \n";
            ss << eigen_obj.format(fmt);
            return ss.str();
        }
    };
    
    // We must manually specialize for Eigen::Transform as it doesn't derive from MatrixBase.
    // It's defined as: Eigen::Transform<Scalar, Dim, Mode, Options>
    template <typename Scalar, int Dim, int Mode, int Options>
    struct StringMaker<Eigen::Transform<Scalar, Dim, Mode, Options>> {
        static std::string convert(const Eigen::Transform<Scalar, Dim, Mode, Options>& trafo) {
            // We print the underlying matrix 
            const auto& matrix = trafo.matrix();

            // Newline at end of rows
            Eigen::IOFormat fmt(4, 0, ", ", "\n", "[", "]");
            std::stringstream ss;
            
            ss << "Transform<Mode=" << Mode << ", Dim=" << Dim << "> = \n"; 
            ss << matrix.format(fmt);
            return ss.str();
        }
    };
    
    // Quaternions also need an explicit specialization
    template <typename Scalar, int Options>
    struct StringMaker<Eigen::Quaternion<Scalar, Options>> {
        static std::string convert(const Eigen::Quaternion<Scalar, Options>& quat) {
            std::stringstream ss;
            ss << "Quaternion(w=" << quat.w() << ", x=" << quat.x() << ", y=" << quat.y() << ", z=" << quat.z() << ")";
            return ss.str();
        }
    };
} // end namespace Catch

#include <catch2/catch_all.hpp>

using namespace Slic3r;

namespace {

namespace fs = boost::filesystem;

struct Temp3mfDir {
    fs::path path;

    Temp3mfDir()
    {
        path = fs::temp_directory_path() / fs::unique_path("orcaslicer-3mf-%%%%-%%%%-%%%%");
        fs::create_directories(path);
    }

    ~Temp3mfDir()
    {
        boost::system::error_code ec;
        fs::remove_all(path, ec);
    }
};

struct LoadedBbsArtifacts {
    PlateDataPtrs         plates;
    std::vector<Preset *> presets;

    ~LoadedBbsArtifacts()
    {
        release_PlateData_list(plates);
        for (Preset *preset : presets)
            delete preset;
    }
};

std::vector<unsigned char> read_file_bytes(const fs::path &path)
{
    std::ifstream input(path.string(), std::ios::binary);
    REQUIRE(input.good());
    return std::vector<unsigned char>(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

std::string read_zip_entry(const fs::path &archive_path, const std::string &entry_name)
{
    mz_zip_archive archive {};
    REQUIRE(open_zip_reader(&archive, archive_path.string()));

    const int index = mz_zip_reader_locate_file(&archive, entry_name.c_str(), nullptr, 0);
    REQUIRE(index >= 0);

    mz_zip_archive_file_stat stat {};
    REQUIRE(mz_zip_reader_file_stat(&archive, index, &stat));
    std::string data(static_cast<size_t>(stat.m_uncomp_size), '\0');
    REQUIRE(mz_zip_reader_extract_to_mem(&archive, index, data.data(), data.size(), 0));
    REQUIRE(close_zip_reader(&archive));
    return data;
}

void rewrite_zip_entry(const fs::path &archive_path, const std::string &entry_name,
                       const std::function<void(std::string &)> &rewrite)
{
    struct Entry {
        std::string                name;
        std::vector<unsigned char> data;
    };

    mz_zip_archive archive {};
    REQUIRE(open_zip_reader(&archive, archive_path.string()));

    std::vector<Entry> entries;
    const mz_uint      entry_count = mz_zip_reader_get_num_files(&archive);
    for (mz_uint i = 0; i < entry_count; ++i) {
        mz_zip_archive_file_stat stat {};
        REQUIRE(mz_zip_reader_file_stat(&archive, i, &stat));
        if (mz_zip_reader_is_file_a_directory(&archive, i))
            continue;

        Entry entry;
        entry.name = stat.m_filename;
        entry.data.resize(static_cast<size_t>(stat.m_uncomp_size));
        REQUIRE(mz_zip_reader_extract_to_mem(&archive, i, entry.data.data(), entry.data.size(), 0));
        if (entry.name == entry_name) {
            std::string text(entry.data.begin(), entry.data.end());
            rewrite(text);
            entry.data.assign(text.begin(), text.end());
        }
        entries.emplace_back(std::move(entry));
    }
    REQUIRE(close_zip_reader(&archive));

    const fs::path replacement = archive_path.parent_path() / (archive_path.stem().string() + "-replacement.3mf");
    {
        Zipper output(replacement.string());
        for (const Entry &entry : entries)
            output.add_entry(entry.name, entry.data.data(), entry.data.size());
        output.finalize();
    }
    fs::copy_file(replacement, archive_path, fs::copy_options::overwrite_existing);
    fs::remove(replacement);
}

} // namespace


SCENARIO("Reading 3mf file", "[3mf]") {
    GIVEN("umlauts in the path of the file") {
        Model model;
        WHEN("3mf model is read") {
            std::string path = std::string(TEST_DATA_DIR) + "/test_3mf/Geräte/Büchse.3mf";
            DynamicPrintConfig config;
            ConfigSubstitutionContext ctxt{ ForwardCompatibilitySubstitutionRule::Disable };
            bool ret = load_3mf(path.c_str(), config, ctxt, &model, false);
            THEN("load should succeed") {
                REQUIRE(ret);
            }
        }
    }
}

TEST_CASE("Legacy 3MF configuration migrates only in memory until explicitly saved", "[3mf][Compatibility]")
{
    static constexpr const char *project_config_entry = "Metadata/project_settings.config";
    static constexpr const char *unknown_key          = "future_orcaslicer_setting";

    Temp3mfDir temp_dir;
    const fs::path legacy_file   = temp_dir.path / "legacy-source.3mf";
    const fs::path migrated_file = temp_dir.path / "migrated-output.3mf";

    Model model;
    model.set_backup_path((temp_dir.path / "source-backup").string());
    REQUIRE(load_stl((std::string(TEST_DATA_DIR) + "/test_3mf/Prusa.stl").c_str(), &model));
    model.add_default_instances();

    DynamicPrintConfig source_config;
    source_config.set_key_value("enable_prime_tower", new ConfigOptionBool(true));
    source_config.set_key_value("support_angle", new ConfigOptionFloat(37.0));
    const std::string legacy_path = legacy_file.string();
    StoreParams       source_store;
    source_store.path     = legacy_path.c_str();
    source_store.model    = &model;
    source_store.config   = &source_config;
    source_store.strategy = SaveStrategy::Zip64;
    REQUIRE(store_bbs_3mf(source_store));

    rewrite_zip_entry(legacy_file, project_config_entry, [](std::string &config_text) {
        const std::string current = "\"enable_prime_tower\"";
        const size_t      pos     = config_text.find(current);
        REQUIRE(pos != std::string::npos);
        config_text.replace(pos, current.size(), "\"enable_wipe_tower\"");
        const size_t object_end = config_text.find_last_of('}');
        REQUIRE(object_end != std::string::npos);
        config_text.insert(object_end, ",\n\t\"future_orcaslicer_setting\": \"123\"\n");
    });

    const std::vector<unsigned char> source_before = read_file_bytes(legacy_file);
    CHECK(read_zip_entry(legacy_file, project_config_entry).find("\"enable_wipe_tower\"") != std::string::npos);

    Model                     loaded_model;
    loaded_model.set_backup_path((temp_dir.path / "loaded-backup").string());
    DynamicPrintConfig        migrated_config;
    ConfigSubstitutionContext substitutions { ForwardCompatibilitySubstitutionRule::Disable };
    LoadedBbsArtifacts        loaded_artifacts;
    bool                      is_bbl_3mf  = false;
    bool                      is_orca_3mf = false;
    Semver                    file_version;
    REQUIRE(load_bbs_3mf(legacy_path.c_str(), &migrated_config, &substitutions, &loaded_model,
                         &loaded_artifacts.plates, &loaded_artifacts.presets, &is_bbl_3mf, &is_orca_3mf,
                         &file_version, nullptr, LoadStrategy::LoadModel | LoadStrategy::LoadConfig));

    CHECK(read_file_bytes(legacy_file) == source_before);
    const auto loaded_keys = migrated_config.keys();
    CAPTURE(loaded_keys, substitutions.unrecogized_keys);
    REQUIRE(migrated_config.option("enable_prime_tower") != nullptr);
    CHECK(migrated_config.opt_bool("enable_prime_tower"));
    CHECK(migrated_config.option("enable_wipe_tower") == nullptr);
    CHECK(migrated_config.opt_float("support_angle") == Catch::Approx(37.0));
    CHECK(substitutions.unrecogized_keys == std::vector<std::string> { unknown_key });

    const std::string migrated_path = migrated_file.string();
    StoreParams       migrated_store;
    migrated_store.path     = migrated_path.c_str();
    migrated_store.model    = &loaded_model;
    migrated_store.config   = &migrated_config;
    migrated_store.strategy = SaveStrategy::Zip64;
    REQUIRE(store_bbs_3mf(migrated_store));
    CHECK(read_file_bytes(legacy_file) == source_before);

    const std::string saved_config = read_zip_entry(migrated_file, project_config_entry);
    CHECK(saved_config.find("\"enable_prime_tower\"") != std::string::npos);
    CHECK(saved_config.find("enable_wipe_tower") == std::string::npos);
    CHECK(saved_config.find(unknown_key) == std::string::npos);

    Model                     roundtrip_model;
    roundtrip_model.set_backup_path((temp_dir.path / "roundtrip-backup").string());
    DynamicPrintConfig        roundtrip_config;
    ConfigSubstitutionContext roundtrip_substitutions { ForwardCompatibilitySubstitutionRule::Disable };
    LoadedBbsArtifacts        roundtrip_artifacts;
    bool                      roundtrip_is_bbl  = false;
    bool                      roundtrip_is_orca = false;
    Semver                    roundtrip_version;
    REQUIRE(load_bbs_3mf(migrated_path.c_str(), &roundtrip_config, &roundtrip_substitutions, &roundtrip_model,
                         &roundtrip_artifacts.plates, &roundtrip_artifacts.presets, &roundtrip_is_bbl,
                         &roundtrip_is_orca, &roundtrip_version, nullptr,
                         LoadStrategy::LoadModel | LoadStrategy::LoadConfig));
    REQUIRE(roundtrip_config.option("enable_prime_tower") != nullptr);
    CHECK(roundtrip_config.opt_bool("enable_prime_tower"));
    CHECK(roundtrip_config.opt_float("support_angle") == Catch::Approx(37.0));
    CHECK(roundtrip_substitutions.unrecogized_keys.empty());
    CHECK(roundtrip_model.objects.size() == loaded_model.objects.size());
}

SCENARIO("Export+Import geometry to/from 3mf file cycle", "[3mf]") {
    GIVEN("world vertices coordinates before save") {
        // load a model from stl file
        Model src_model;
        std::string src_file = std::string(TEST_DATA_DIR) + "/test_3mf/Prusa.stl";
        load_stl(src_file.c_str(), &src_model);
        src_model.add_default_instances();

        ModelObject* src_object = src_model.objects.front();

        // apply generic transformation to the 1st volume
        Geometry::Transformation src_volume_transform;
        src_volume_transform.set_offset({ 10.0, 20.0, 0.0 });
        src_volume_transform.set_rotation({ Geometry::deg2rad(25.0), Geometry::deg2rad(35.0), Geometry::deg2rad(45.0) });
        src_volume_transform.set_scaling_factor({ 1.1, 1.2, 1.3 });
        src_volume_transform.set_mirror({ -1.0, 1.0, -1.0 });
        src_object->volumes.front()->set_transformation(src_volume_transform);

        // apply generic transformation to the 1st instance
        Geometry::Transformation src_instance_transform;
        src_instance_transform.set_offset({ 5.0, 10.0, 0.0 });
        src_instance_transform.set_rotation({ Geometry::deg2rad(12.0), Geometry::deg2rad(13.0), Geometry::deg2rad(14.0) });
        src_instance_transform.set_scaling_factor({ 0.9, 0.8, 0.7 });
        src_instance_transform.set_mirror({ 1.0, -1.0, -1.0 });
        src_object->instances.front()->set_transformation(src_instance_transform);

        WHEN("model is saved+loaded to/from 3mf file") {
            // save the model to 3mf file
            std::string test_file = std::string(TEST_DATA_DIR) + "/test_3mf/prusa.3mf";
            store_3mf(test_file.c_str(), &src_model, nullptr, false);

            // load back the model from the 3mf file
            Model dst_model;
            DynamicPrintConfig dst_config;
            {
                ConfigSubstitutionContext ctxt{ ForwardCompatibilitySubstitutionRule::Disable };
                load_3mf(test_file.c_str(), dst_config, ctxt, &dst_model, false);
            }
            boost::filesystem::remove(test_file);

            // compare meshes
            TriangleMesh src_mesh = src_model.mesh();
            TriangleMesh dst_mesh = dst_model.mesh();

            bool res = src_mesh.its.vertices.size() == dst_mesh.its.vertices.size();
            if (res) {
                for (size_t i = 0; i < dst_mesh.its.vertices.size(); ++i) {
                    res &= dst_mesh.its.vertices[i].isApprox(src_mesh.its.vertices[i]);
                }
            }
            THEN("world vertices coordinates after load match") {
                REQUIRE(res);
            }
        }
    }
}

SCENARIO("2D convex hull of sinking object", "[3mf][.]") {
    GIVEN("model") {
        // load a model
        Model model;
        std::string src_file = std::string(TEST_DATA_DIR) + "/test_3mf/Prusa.stl";
        REQUIRE(load_stl(src_file.c_str(), &model));
        model.add_default_instances();

        WHEN("model is rotated, scaled and set as sinking") {
            ModelObject* object = model.objects[0];
            object->center_around_origin(false);

	    // This outputs the same exact data as the Prusaslicer test
	    object->volumes[0]->mesh().write_ascii("/tmp/orca.ascii");

            // set instance's attitude so that it is rotated, scaled (and sinking? how is it sinking? the rotation? does it matter if it's sinking?)
            ModelInstance* instance = object->instances[0];
            instance->set_rotation(X, -M_PI / 4.0);
            instance->set_offset(Vec3d::Zero());
            instance->set_scaling_factor({ 2.0, 2.0, 2.0 });

            // calculate 2D convex hull
	    auto trafo = instance->get_transformation().get_matrix();

	    // This matrix is the same exact matrix as the Prusaslicer test
	    CAPTURE(trafo);
            Polygon hull_2d = object->convex_hull_2d(trafo);

	    // But we get different hull_2d.points here (and somehow decimal numbers despite being int64_t values, but that's probabaly printing configuration somewhere -- Prusaslicer's prints out with newlines between the X&Y and not one between coordinates, which is about the worse possible output).
	    // I think it's something to do with PrusaSlicer ignoring everything under the Z plane, which makes sense from the results.
	    // See the comments added to ModelObject::convex_hull_2d for more information.

            // verify result
            Points result = {
                { -91501496, -15914144 },
                { 91501496, -15914144 },
                { 91501496, 4243 },
                { 78229680, 4246883 },
                { 56898100, 4246883 },
                { -85501496, 4242641 },
                { -91501496, 4243 }
            };

            THEN("2D convex hull should match with reference") {
                // Allow 1um error due to floating point rounding.
                bool res = hull_2d.points.size() == result.size();
                if (res) {
                    for (size_t i = 0; i < result.size(); ++ i) {
                        const Point &p1 = result[i];
                        const Point &p2 = hull_2d.points[i];
                        CHECK((std::abs(p1.x() - p2.x()) > 1 || std::abs(p1.y() - p2.y()) > 1));
                    }
                }

                CAPTURE(hull_2d.points);
                REQUIRE(res);
            }
        }
    }
}
