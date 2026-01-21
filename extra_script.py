Import("env")
import os
import sys

def generate_compile_commands(source, target, env):
    # Determine the path to the pio executable
    # Typically it's in the same directory as the python executable in the penv
    pio_path = os.path.join(os.path.dirname(sys.executable), "pio")
    if os.name == 'nt':
        pio_path += ".exe"
    
    print(f"Generating compile_commands.json using: {pio_path}")
    
    # Run the compiledb target
    # We use env.Execute to run it as a shell command with the full path
    env.Execute(f'"{pio_path}" run -t compiledb')

# Register the post-action to run after the firmware image is built
env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", generate_compile_commands)