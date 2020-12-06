dir_impl1=~/NetFPGA-SUME-live/projects/partial_reconfiguration_RP4/hw/project/reference_switch_lite.runs/impl_1
dir_child1=~/NetFPGA-SUME-live/projects/partial_reconfiguration_RP4/hw/project/reference_switch_lite.runs/child1
dir_child2=~/NetFPGA-SUME-live/projects/partial_reconfiguration_RP4/hw/project/reference_switch_lite.runs/child2

for i in $(seq 0 3)
do
  ln -s ${dir_impl1}/nf_datapath_0_reconfigurable_partition_${i}_udp_echo_back_partial.bit bit/0000002${i}.bit
done

for i in $(seq 0 3)
do
  ln -s ${dir_child1}/nf_datapath_0_reconfigurable_partition_${i}_path_through_partial.bit bit/0000001${i}.bit
done

for i in $(seq 0 3)
do
  ln -s  ${dir_child2}/nf_datapath_0_reconfigurable_partition_${i}_udp_echo_back_replacing_A_with_B_partial.bit bit/0000003${i}.bit
done
