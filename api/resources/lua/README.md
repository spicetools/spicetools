# Lua Scripting
Supported version: Lua 5.4.3
No proper documentation yet. Check the example scripts if you need this!
For undocumented functions you can find the definitions in the source code (script/api/*.cpp).
They are very similar to what the network API provides.

# Automatic Execution
Create a "scripts" folder next to spice and put your scripts in there (subfolders allowed).
The prefix specifies when the script will be called:

- `boot_*`: executed on game boot
- `shutdown_*`: executed on game end
- `config_*`: executed when you start spicecfg (mostly for debugging/tests)

Example: "scripts/boot_patch.py" would be called on game boot.
