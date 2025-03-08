import importlib
import os
import sys

def get_package_from_path(path):
    linux_path = path.replace("\\", "/")
    return path.replace("/", ".")


def is_plugin_path(plugin_path, plugin_file):
    return os.path.isdir(plugin_path) and plugin_file in os.listdir(plugin_path)


def get_plugin(plugin_name, plugins_path, plugin_file):
    plugin_path = os.path.join(plugins_path, plugin_name)
    if not is_plugin_path(plugin_path, plugin_file):
        print("ERROR: Cannot find " + plugin_file + " in " + plugin_path)
        sys.exit(255)
    plugin_file_path = os.path.join(plugin_path, plugin_file)
    package_name = get_package_from_path(plugin_path)
    module_name = ".".join([package_name, plugin_name])
    spec = importlib.util.spec_from_file_location(module_name, plugin_file_path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module
    spec.loader.exec_module(module)
    return module


def get_plugins(plugins_path, plugin_file):
    plugins = []
    plugins_list = sorted(os.listdir(plugins_path))
    for plugin_name in plugins_list:
        plugin_path = os.path.join(plugins_path, plugin_name)
        if not is_plugin_path(plugin_path, plugin_file):
            continue
        plugin = get_plugin(plugin_name, plugins_path, plugin_file)
        plugins.append(plugin)
    return plugins
