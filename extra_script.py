# extra_script.py - Dynamically set source folder per environment

Import("env")

# Map environment name → source folder
SRC_FOLDERS = {
    "avr128db28": "avr_firmware/src",
    "esp32dev":   "esp32_firmware/src"
    # Add more envs here if needed
}

env_name = env["PIOENV"]  # Current environment being built

if env_name in SRC_FOLDERS:
    custom_src = env["PROJECT_DIR"] + "/" + SRC_FOLDERS[env_name]
    env.Replace(PROJECT_SRC_DIR = custom_src)
    print(f"Using custom src folder for {env_name}: {custom_src}")
else:
    print(f"Warning: No custom src folder defined for env '{env_name}'")