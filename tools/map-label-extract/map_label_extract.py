import bpy
import toml
import re

def remove_blender_name_postfix(name):
    return re.sub(r'\.\d{3}', '', name)

worldmap_config = {}

active_collection = bpy.context.view_layer.active_layer_collection.collection
objects = active_collection.objects
for object in objects:
    if object.parent is None:
        for child in object.children:
            for label in child.children:
                real_child_name = remove_blender_name_postfix(child.name)
                real_label = remove_blender_name_postfix(label.name)
                if real_child_name not in worldmap_config:
                    worldmap_config[real_child_name] = {}
                worldmap_config[real_child_name][real_label] = {'x': -label.location.y, 'y': label.location.z, 'z': label.location.x}
    
with open(bpy.path.abspath('//worldmap.toml'), 'w') as f:
    toml.dump(worldmap_config, f)
