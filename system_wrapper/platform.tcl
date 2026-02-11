platform generate -domains 
platform generate
platform active {system_wrapper}
domain active {domain_ps7_cortexa9_0}
domain active {zynq_fsbl}
bsp reload
bsp reload
platform generate
platform generate
platform generate
platform active {system_wrapper}
platform config -updatehw {C:/Users/mumbl/Downloads/SPARC-hw.xpr/hw/system_wrapper.xsa}
platform generate -domains 
