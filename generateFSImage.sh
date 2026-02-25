# This command forces the build to run the image generation script.
# It uses the LittleFS component's script (which you must have downloaded via idf.py build).

# 1. Find the script: It's located in the managed_components directory.
LITTLEFS_SCRIPT=$(find managed_components -name littlefs_image_gen.py)

# 2. Define the output file path.
OUTPUT_FILE="build/littlefs_image.bin"

# 3. Execute the Python script:
python $LITTLEFS_SCRIPT \
    --input-dir "main/internal_data" \
    --partition-size 1048576 \
    --output $OUTPUT_FILE