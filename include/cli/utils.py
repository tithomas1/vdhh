import os


def name_from_file_path(file_path):
    d, f = os.path.split(file_path)
    name = f
    if '.' in name:
        name = name.split('.').pop(0)
    return name
