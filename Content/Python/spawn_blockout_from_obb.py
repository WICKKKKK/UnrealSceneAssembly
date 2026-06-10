"""Spawn BlockoutBox actors from selected StaticMeshActor OBBs.

Run this script in the Unreal Editor after selecting one or more StaticMeshActor
instances. Existing generated actors tagged with CLEANUP_TAG are removed first.
"""

import hashlib

import unreal

import config
import ingest_static_meshes as backend


CLEANUP_TAG = "OBBGeneratedBlockout"
MIN_BOX_SIZE_CM = 1.0
QUERY_BATCH_SIZE = 1000
LOG_PREFIX = "[SpawnBlockoutFromOBB]"


def _log(message):
    unreal.log("{0} {1}".format(LOG_PREFIX, message))


def _warn(message):
    unreal.log_warning("{0} {1}".format(LOG_PREFIX, message))


def _editor_actor_subsystem():
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not subsystem:
        raise RuntimeError("EditorActorSubsystem is unavailable.")
    return subsystem


def _actor_tags(actor):
    try:
        return [str(tag) for tag in actor.get_editor_property("tags")]
    except Exception:
        return [str(tag) for tag in getattr(actor, "tags", [])]


def _set_actor_tags(actor, tags):
    unique = []
    for tag in tags:
        value = str(tag).strip()
        if value and value not in unique:
            unique.append(value)
    actor.set_editor_property("tags", unique)


def _selected_static_mesh_actors(subsystem):
    selected = subsystem.get_selected_level_actors()
    return [actor for actor in selected if isinstance(actor, unreal.StaticMeshActor)]


def _static_mesh_component(actor):
    getter = getattr(actor, "get_static_mesh_component", None)
    if callable(getter):
        try:
            component = getter()
        except Exception:
            component = None
        if component:
            return component

    component = getattr(actor, "static_mesh_component", None)
    if not component:
        try:
            component = actor.get_editor_property("static_mesh_component")
        except Exception:
            component = None
    return component


def _static_mesh_asset(actor):
    component = _static_mesh_component(actor)
    if not component:
        return None
    getter = getattr(component, "get_static_mesh", None)
    if callable(getter):
        mesh = getter()
        if mesh:
            return mesh
    return component.get_editor_property("static_mesh")


def _static_mesh_path(actor):
    mesh = _static_mesh_asset(actor)
    if not mesh:
        return None
    get_path_name = getattr(mesh, "get_path_name", None)
    if not callable(get_path_name):
        return None
    path = get_path_name()
    return path if path and path != "None" else None


def _asset_id(asset_path):
    return hashlib.md5(asset_path.encode("utf-8")).hexdigest()


def _chunks(values, size):
    for index in range(0, len(values), size):
        yield values[index:index + size]


def _query_ai_tags_batch(asset_ids):
    if not asset_ids:
        return {}

    payload = {
        "filters": {
            "project_names": [config.PROJECT_NAME],
            "asset_ids": asset_ids,
        },
        "output_fields": ["asset_id", "asset_path", "ai_tags"],
        "limit": len(asset_ids),
        "offset": 0,
        "order_by": ["asset_path:asc"],
    }
    _, envelope = backend.request_json(
        "POST",
        "/milvus/collections/{0}/query_assets".format(config.COLLECTION_CLIP),
        payload,
    )
    records = backend.unwrap_data(envelope) or []

    by_asset_id = {}
    for record in records:
        if not isinstance(record, dict):
            continue
        asset_path = str(record.get("asset_path") or "")
        record_asset_id = str(record.get("asset_id") or "")
        if not record_asset_id and asset_path:
            record_asset_id = _asset_id(asset_path)
        if not record_asset_id:
            continue
        raw_tags = record.get("ai_tags") or []
        if isinstance(raw_tags, str):
            raw_tags = [raw_tags]
        elif not isinstance(raw_tags, (list, tuple, set)):
            raw_tags = [raw_tags]
        tags = [str(tag).strip() for tag in raw_tags if str(tag).strip()]
        by_asset_id[record_asset_id] = tags
    return by_asset_id


def _query_ai_tags(asset_ids):
    unique_asset_ids = sorted(set(asset_ids))
    by_asset_id = {}
    for batch in _chunks(unique_asset_ids, QUERY_BATCH_SIZE):
        by_asset_id.update(_query_ai_tags_batch(batch))
    return by_asset_id


def _rotator_to_quat(rotator):
    converter = getattr(rotator, "quaternion", None)
    if callable(converter):
        return converter()
    converter = getattr(rotator, "to_quat", None)
    if callable(converter):
        return converter()
    return rotator


def _get_transform_rotation(transform):
    get_rotation = getattr(transform, "get_rotation", None)
    rotation = get_rotation() if callable(get_rotation) else getattr(transform, "rotation", None)
    if rotation is None:
        try:
            rotation = transform.get_editor_property("rotation")
        except Exception:
            rotation = None

    if rotation is None:
        rotator = unreal.Rotator(0.0, 0.0, 0.0)
        return rotator, _rotator_to_quat(rotator)
    if isinstance(rotation, unreal.Rotator):
        return rotation, _rotator_to_quat(rotation)
    rotator = rotation.rotator()
    return rotator, rotation


def _get_transform_translation(transform):
    getter = getattr(transform, "get_translation", None)
    if callable(getter):
        return getter()
    translation = getattr(transform, "translation", None)
    if translation is not None:
        return translation
    try:
        return transform.get_editor_property("translation")
    except Exception:
        return unreal.Vector(0.0, 0.0, 0.0)


def _transform_position(transform, position):
    transform_position = getattr(transform, "transform_position", None)
    if callable(transform_position):
        return transform_position(position)
    translation = _get_transform_translation(transform)
    _, quat = _get_transform_rotation(transform)
    rotated = quat.rotate_vector(position)
    return unreal.Vector(translation.x + rotated.x, translation.y + rotated.y, translation.z + rotated.z)


def _rotate_vector(quat, vector):
    rotate_vector = getattr(quat, "rotate_vector", None)
    if callable(rotate_vector):
        return rotate_vector(vector)
    rotated = quat * vector
    return rotated


def _vector_subtract(left, right):
    return unreal.Vector(left.x - right.x, left.y - right.y, left.z - right.z)


def _clamp_box_size(value):
    return max(float(value), MIN_BOX_SIZE_CM)


def _set_blockout_box_size(actor, size_x, size_y, size_z):
    box_size = actor.get_editor_property("box_size")
    for axis, value in (("x", size_x), ("y", size_y), ("z", size_z)):
        blockout_float = box_size.get_editor_property(axis)
        blockout_float.set_editor_property("value", float(value))
        box_size.set_editor_property(axis, blockout_float)
    actor.set_editor_property("box_size", box_size)


def _update_blockout(actor):
    updater = getattr(actor, "update_current_blockout", None)
    if callable(updater):
        updater()
        return
    requester = getattr(actor, "request_update_current", None)
    if callable(requester):
        requester()


def _set_semantic_tags(actor, ai_tags):
    component_class = getattr(unreal, "SceneSemanticComponent", None)
    if component_class is None:
        raise RuntimeError("SceneSemanticComponent is unavailable. Rebuild/reload the UnrealSceneAssembly plugin.")
    component = component_class.set_actor_semantic(actor, "", "", ai_tags)
    if not component:
        raise RuntimeError("Failed to add or update SceneSemanticComponent on {0}.".format(actor.get_name()))


def _add_cleanup_tag(actor):
    tags = _actor_tags(actor)
    if CLEANUP_TAG not in tags:
        tags.append(CLEANUP_TAG)
    _set_actor_tags(actor, tags)


def _blockout_box_class():
    actor_class = getattr(unreal, "BlockoutBox", None)
    if actor_class:
        return actor_class
    actor_class = unreal.load_class(None, "/Script/Blockout.BlockoutBox")
    if not actor_class:
        raise RuntimeError("BlockoutBox class is unavailable. Rebuild/reload the Blockout module.")
    return actor_class


def _destroy_previous_generated(subsystem):
    destroyed = 0
    for actor in list(subsystem.get_all_level_actors()):
        if CLEANUP_TAG in _actor_tags(actor):
            if subsystem.destroy_actor(actor):
                destroyed += 1
    return destroyed


def _make_label(source_actor):
    label = source_actor.get_actor_label()
    return "BlockoutBox_{0}".format(label or source_actor.get_name())


def _spawn_box_for_actor(subsystem, box_class, source_actor, ai_tags):
    obb = unreal.SceneAssemblySolverLibrary.get_actor_obb(source_actor)
    half_extents = obb.half_extents
    transform = obb.world_transform
    rotation, quat = _get_transform_rotation(transform)

    size_x = _clamp_box_size(half_extents.x * 2.0)
    size_y = _clamp_box_size(half_extents.y * 2.0)
    size_z = _clamp_box_size(half_extents.z * 2.0)

    local_center = obb.local_center
    obb_bottom_center_local = unreal.Vector(
        local_center.x, local_center.y, local_center.z - half_extents.z
    )
    obb_bottom_center_world = _transform_position(transform, obb_bottom_center_local)
    box_local_bottom_center = unreal.Vector(size_x * 0.5, size_y * 0.5, 0.0)
    location = _vector_subtract(
        obb_bottom_center_world, _rotate_vector(quat, box_local_bottom_center)
    )

    try:
        box = subsystem.spawn_actor_from_class(box_class, location, rotation)
    except TypeError:
        box = subsystem.spawn_actor_from_class(box_class, location)
        if box:
            box.set_actor_rotation(rotation, False)

    if not box:
        raise RuntimeError("Failed to spawn BlockoutBox for {0}.".format(source_actor.get_actor_label()))

    box.set_actor_label(_make_label(source_actor))
    _set_blockout_box_size(box, size_x, size_y, size_z)
    _update_blockout(box)
    _set_semantic_tags(box, ai_tags)
    _add_cleanup_tag(box)
    return box


def main():
    subsystem = _editor_actor_subsystem()
    selected_actors = _selected_static_mesh_actors(subsystem)
    if not selected_actors:
        _warn("No selected StaticMeshActor found. Select one or more StaticMeshActor instances and run again.")
        return {"selected": 0, "spawned": 0, "destroyed": 0, "missing_tags": []}

    actor_infos = []
    skipped = []
    for actor in selected_actors:
        asset_path = _static_mesh_path(actor)
        if not asset_path:
            skipped.append(actor.get_actor_label())
            continue
        actor_infos.append({"actor": actor, "asset_path": asset_path, "asset_id": _asset_id(asset_path)})

    if not actor_infos:
        _warn("Selected StaticMeshActor instances have no valid StaticMesh asset.")
        return {"selected": len(selected_actors), "spawned": 0, "destroyed": 0, "missing_tags": [], "skipped": skipped}

    backend.auth_self_check()
    tags_by_asset_id = _query_ai_tags([info["asset_id"] for info in actor_infos])
    box_class = _blockout_box_class()

    spawned = []
    missing_tags = []
    with unreal.ScopedEditorTransaction("Spawn BlockoutBox From StaticMesh OBB"):
        destroyed = _destroy_previous_generated(subsystem)
        for info in actor_infos:
            ai_tags = tags_by_asset_id.get(info["asset_id"], [])
            if not ai_tags:
                missing_tags.append(info["asset_path"])
            spawned.append(_spawn_box_for_actor(subsystem, box_class, info["actor"], ai_tags))

    _log(
        "Finished. selected={0}, spawned={1}, destroyed_previous={2}, skipped={3}, missing_ai_tags={4}".format(
            len(selected_actors),
            len(spawned),
            destroyed,
            len(skipped),
            len(missing_tags),
        )
    )
    for label in skipped:
        _warn("Skipped actor without valid StaticMesh asset: {0}".format(label))
    for asset_path in missing_tags:
        _warn("No ai_tags found in Milvus for asset: {0}".format(asset_path))

    return {
        "selected": len(selected_actors),
        "spawned": len(spawned),
        "destroyed": destroyed,
        "skipped": skipped,
        "missing_tags": missing_tags,
    }


if __name__ == "__main__":
    main()
