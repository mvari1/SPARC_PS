# --------------------------------------------
# Portable Vitis 2023.1 Project Creation Script
# Zybo Z7-20
# --------------------------------------------

# Create local workspace inside repo
setws ./workspace

# Create platform from repo XSA
platform create -name SPARC_platform \
    -hw ./xsa/system_wrapper.xsa \
    -proc ps7_cortexa9_0 \
    -os standalone

# Build platform
platform generate

# Create application
app create -name SPARC \
    -platform SPARC_platform \
    -domain standalone_domain \
    -template {Empty Application (C++)} \
    -lang c++

# Import source files
importsources -name SPARC -path ./src

# Build application
app build -name SPARC

puts "Project successfully created."