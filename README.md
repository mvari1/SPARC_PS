# SPARC_PS

# SPARC Vitis 2023.1 Portable Project

This repository contains a fully portable Vitis 2023.1 project for the Zybo Z7-20.

The project is recreated entirely from:
- system_wrapper.xsa
- Application source code
- A TCL recreate script

No Vitis workspace files are committed.

------------------------------------------------------------
PREREQUISITES
------------------------------------------------------------

- Vitis 2023.1 installed
- Vivado 2023.1 installed
- xsct available in system PATH

If xsct is not available, add:

C:\Xilinx\Vitis\2023.1\bin

to your Windows PATH environment variable.

Verify installation:

xsct -version


------------------------------------------------------------
HOW TO RECREATE THE PROJECT
------------------------------------------------------------

1. Open Command Prompt

2. Navigate to repository root:

cd C:\path\to\SPARC_PS

3. Run the recreate script:

xsct scripts/create_vitis_project.tcl

This will automatically:

- Create a local workspace folder
- Create the platform from xsa/system_wrapper.xsa
- Build the platform
- Create the SPARC application
- Import source files
- Build the ELF

After completion you will see:

workspace/
    SPARC_platform/
    SPARC/


------------------------------------------------------------
HOW TO OPEN IN VITIS GUI
------------------------------------------------------------

1. Open Vitis 2023.1

2. Select "Open Workspace"

3. Browse to:

<repo root>/workspace

4. Click Open

You will now see:
- SPARC_platform
- SPARC

Build and program as normal.
