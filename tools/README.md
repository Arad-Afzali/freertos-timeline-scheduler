# Configuration Generator

Python tool for creating FreeRTOS scheduler configurations.

## How to use

```bash
# See help
python3 config_generator.py -h

# Create example config
python3 config_generator.py --example -s my_config.json

# Load and check a config
python3 config_generator.py -f my_config.json

# Generate C code
python3 config_generator.py -f my_config.json -g

# Interactive mode
python3 config_generator.py -i -s new_config.json -g
```

## What it does

1. Loads task configuration from JSON
2. Checks for errors (overlapping HRT tasks, invalid times, etc.)
3. Generates C code that works with our scheduler

## Output files

- `generated_config.h` - header file
- `generated_config.c` - configuration array
- `task_stubs.c` - empty task functions to fill in
