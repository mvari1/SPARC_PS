# Usage with Vitis IDE:
# In Vitis IDE create a Single Application Debug launch configuration,
# change the debug type to 'Attach to running target' and provide this 
# tcl script in 'Execute Script' option.
# This script is written to be workspace-relative. It computes the
# repository root at runtime and builds file paths from there so the
# project can be relocated without editing absolute paths.
# Usage with xsct: source ./debugger_zybo-z7-20-pcam-5c-default.tcl

# compute script dir and repository root (three levels up)
set script_dir [file dirname [info script]]
set repo_root [file normalize [file join $script_dir ../../..]]

# project-specific paths (workspace-relative)
set bitfile [file normalize [file join $repo_root Zybo-Z7-20-pcam-5c _ide bitstream system_wrapper.bit]]
set xsa [file normalize [file join $repo_root system_wrapper export system_wrapper hw system_wrapper.xsa]]
set xpfm [file normalize [file join $repo_root system_wrapper export system_wrapper system_wrapper.xpfm]]
set ps7init [file normalize [file join $repo_root Zybo-Z7-20-pcam-5c _ide psinit ps7_init.tcl]]
set elf [file normalize [file join $repo_root Zybo-Z7-20-pcam-5c Debug Zybo-Z7-20-pcam-5c.elf]]
# 
connect -url tcp:127.0.0.1:3121
targets -set -nocase -filter {name =~"APU*"}
rst -system
after 3000
targets -set -filter {jtag_cable_name =~ "Digilent Zybo Z7 210351BDF90DA" && level==0 && jtag_device_ctx=="jsn-Zybo Z7-210351BDF90DA-23727093-0"}
fpga -file $bitfile
targets -set -nocase -filter {name =~"APU*"}
loadhw -hw $xsa -mem-ranges [list {0x40000000 0xbfffffff}] -regs
configparams force-mem-access 1
targets -set -nocase -filter {name =~"APU*"}
source $ps7init
ps7_init
ps7_post_config
targets -set -nocase -filter {name =~ "*A9*#0"}
dow $elf
configparams force-mem-access 0
bpadd -addr &main
