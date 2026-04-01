import os
from pathlib import Path

ROOT_DIR = Path(__file__).parent
SRC_DIR = ROOT_DIR / "src"
OUTPUT_FILE = ROOT_DIR / "output.txt"

IGNORED_EXTENSIONS = {".png", ".jpg", ".jpeg", ".gif", ".ico", ".pdf"}
IGNORED_DIRS = {"node_modules", ".git", "__pycache__", "dist", "build"}

def build_tree(path: Path, prefix=""):
    tree = []
    items = sorted(path.iterdir(), key=lambda x: (x.is_file(), x.name.lower()))

    for i, item in enumerate(items):
        if item.name in IGNORED_DIRS:
            continue

        connector = "└── " if i == len(items) - 1 else "├── "
        tree.append(f"{prefix}{connector}{item.name}")

        if item.is_dir():
            extension = "    " if i == len(items) - 1 else "│   "
            tree.extend(build_tree(item, prefix + extension))

    return tree


def read_all_files():
    content_blocks = []

    for root, dirs, files in os.walk(SRC_DIR):
        dirs[:] = [d for d in dirs if d not in IGNORED_DIRS]

        for file in files:
            file_path = Path(root) / file

            if file_path.suffix.lower() in IGNORED_EXTENSIONS:
                continue

            try:
                relative_path = file_path.relative_to(ROOT_DIR)

                with open(file_path, "r", encoding="utf-8") as f:
                    content = f.read()

                block = f"""
==================================================
FILE: {relative_path}
==================================================

{content}

"""
                content_blocks.append(block)

            except Exception as e:
                content_blocks.append(
                    f"\n[ERROR READING {file_path}]: {str(e)}\n"
                )

    return content_blocks


def generate_context_file():
    tree_structure = "\n".join(build_tree(SRC_DIR))
    file_contents = "\n".join(read_all_files())

    final_output = f"""
PROJECT CONTEXT
========================================

DIRECTORY TREE
----------------------------------------
{tree_structure}

FILE CONTENTS
----------------------------------------
{file_contents}
"""

    with open(OUTPUT_FILE, "w", encoding="utf-8") as f:
        f.write(final_output)

    print(f"Context written successfully to: {OUTPUT_FILE}")


if __name__ == "__main__":
    generate_context_file()