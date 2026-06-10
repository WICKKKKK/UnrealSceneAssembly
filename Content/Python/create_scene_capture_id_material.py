import unreal


PACKAGE_PATH = "/UnrealSceneAssembly/SceneCapture"
MATERIAL_NAME = "M_SceneCaptureID"
PARAMETER_NAME = "IDColor"


def set_editor_property_if_available(material, property_name, value):
    try:
        material.set_editor_property(property_name, value)
        return True
    except Exception:
        return False


def enable_material_usage(material, property_name, enum_names):
    set_editor_property_if_available(material, property_name, True)

    material_usage_enum = getattr(unreal, "MaterialUsage", None)
    if material_usage_enum is None:
        return

    for enum_name in enum_names:
        usage = getattr(material_usage_enum, enum_name, None)
        if usage is None:
            continue

        try:
            unreal.MaterialEditingLibrary.set_material_usage(material, usage)
            return
        except Exception:
            pass


def create_scene_capture_id_material():
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    editor_asset_library = unreal.EditorAssetLibrary
    material_path = f"{PACKAGE_PATH}/{MATERIAL_NAME}"
    material_object_path = f"{material_path}.{MATERIAL_NAME}"

    if not editor_asset_library.does_directory_exist(PACKAGE_PATH):
        editor_asset_library.make_directory(PACKAGE_PATH)

    material = unreal.load_asset(material_object_path) or unreal.load_asset(material_path)
    if material is None and editor_asset_library.does_asset_exist(material_path):
        material = editor_asset_library.load_asset(material_path)
    if material is None:
        factory = unreal.MaterialFactoryNew()
        material = asset_tools.create_asset(MATERIAL_NAME, PACKAGE_PATH, unreal.Material, factory)

    if material is None:
        raise RuntimeError(f"Failed to create material: {material_path}")

    # Some material properties are not exposed uniformly across UE Python versions.
    for property_name, value in (
        ("shading_model", unreal.MaterialShadingModel.MSM_UNLIT),
        ("blend_mode", unreal.BlendMode.BLEND_OPAQUE),
        ("two_sided", True),
    ):
        set_editor_property_if_available(material, property_name, value)

    # Pre-enable common primitive usages to avoid shader compilation when this
    # material is temporarily assigned across the whole scene for ID captures.
    for property_name, enum_names in (
        ("used_with_static_lighting", ("MATUSAGE_STATIC_LIGHTING", "MATUSAGE_StaticLighting")),
        ("used_with_skeletal_mesh", ("MATUSAGE_SKELETAL_MESH", "MATUSAGE_SkeletalMesh")),
        ("used_with_instanced_static_meshes", ("MATUSAGE_INSTANCED_STATIC_MESHES", "MATUSAGE_InstancedStaticMeshes")),
        ("used_with_spline_meshes", ("MATUSAGE_SPLINE_MESH", "MATUSAGE_SplineMesh")),
        ("used_with_morph_targets", ("MATUSAGE_MORPH_TARGETS", "MATUSAGE_MorphTargets")),
        ("used_with_geometry_collections", ("MATUSAGE_GEOMETRY_COLLECTIONS", "MATUSAGE_GeometryCollections")),
        ("used_with_geometry_cache", ("MATUSAGE_GEOMETRY_CACHE", "MATUSAGE_GeometryCache")),
        ("used_with_hair_strands", ("MATUSAGE_HAIR_STRANDS", "MATUSAGE_HairStrands")),
        ("used_with_water", ("MATUSAGE_WATER", "MATUSAGE_Water")),
        ("used_with_lidar_point_cloud", ("MATUSAGE_LIDAR_POINT_CLOUD", "MATUSAGE_LidarPointCloud")),
        ("used_with_virtual_heightfield_mesh", ("MATUSAGE_VIRTUAL_HEIGHTFIELD_MESH", "MATUSAGE_VirtualHeightfieldMesh")),
        ("used_with_nanite", ("MATUSAGE_NANITE", "MATUSAGE_Nanite")),
        ("used_with_volumetric_cloud", ("MATUSAGE_VOLUMETRIC_CLOUD", "MATUSAGE_VolumetricCloud")),
        ("used_with_heterogeneous_volumes", ("MATUSAGE_HETEROGENEOUS_VOLUMES", "MATUSAGE_HeterogeneousVolumes")),
    ):
        enable_material_usage(material, property_name, enum_names)

    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    color_parameter = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionVectorParameter,
        -300,
        0,
    )
    color_parameter.set_editor_property("parameter_name", PARAMETER_NAME)
    color_parameter.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))

    unreal.MaterialEditingLibrary.connect_material_property(
        color_parameter,
        "",
        unreal.MaterialProperty.MP_EMISSIVE_COLOR,
    )
    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.recompile_material(material)

    editor_asset_library.save_loaded_asset(material)
    unreal.log(f"Created/updated SceneCapture ID material: {material_path}")
    return material


if __name__ == "__main__":
    create_scene_capture_id_material()
