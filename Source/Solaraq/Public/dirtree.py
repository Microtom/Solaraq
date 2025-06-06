import os

# Define the visual elements for the tree
TEE = "├── "
ELBOW = "└── "
PIPE_PREFIX = "│   "
SPACE_PREFIX = "    "

def print_directory_tree(start_path, prefix=""):
    """
    Recursively prints the directory tree.

    Args:
        start_path (str): The path to the directory to start from.
        prefix (str): The prefix string for indentation and tree lines.
    """
    try:
        # Get all items in the current directory, sorted
        # We sort them to ensure a consistent order
        items = sorted(os.listdir(start_path))
    except PermissionError:
        # If we don't have permission to read the directory
        print(f"{prefix}{TEE}[ACCESS DENIED: {os.path.basename(start_path)}]")
        return
    except FileNotFoundError:
        # If the path doesn't exist (e.g., a broken symlink was followed)
        print(f"{prefix}{TEE}[NOT FOUND: {os.path.basename(start_path)}]")
        return

    # Determine the number of items to correctly use ELBOW for the last item
    num_items = len(items)

    for i, item_name in enumerate(items):
        full_path = os.path.join(start_path, item_name)
        is_last_item = (i == num_items - 1)

        # Connector for the current item
        connector = ELBOW if is_last_item else TEE
        print(f"{prefix}{connector}{item_name}")

        # If the item is a directory, recurse into it
        if os.path.isdir(full_path):
            # New prefix for items inside this subdirectory
            # If the current item is the last, use spaces for the next prefix.
            # Otherwise, use a pipe to extend the tree line.
            new_prefix_extension = SPACE_PREFIX if is_last_item else PIPE_PREFIX
            print_directory_tree(full_path, prefix + new_prefix_extension)

if __name__ == "__main__":
    # Get the current working directory (where the script is executed)
    current_directory = os.getcwd()

    # Print the root directory name
    print(f"{os.path.basename(current_directory)}/") # Or just current_directory for full path

    # Start printing the tree from the current directory
    print_directory_tree(current_directory)

    # Example of how to exclude certain directories or files (optional)
    # You would need to modify the print_directory_tree function.
    # For instance, to ignore '.git' and '__pycache__':
    # ignored_items = {'.git', '__pycache__', '.venv'}
    # items = sorted([item for item in os.listdir(start_path) if item not in ignored_items])
    # ... (rest of the logic)