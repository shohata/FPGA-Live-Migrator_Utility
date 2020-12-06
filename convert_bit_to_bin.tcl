for {set i 1} {$i <= 3} {incr i} {
  for {set j 0} {$j < 4} {incr j} {
    puts "000000${i}${j}.bit"
    #write_cfgmem -format BIN -interface SMAPx8 -loadbit "up 0x0 bit/000000${i}${j}.bit" "bin/000000${i}${j}.bin"
    write_cfgmem -format BIN -interface SMAPx8 -disablebitswap -loadbit "up 0x0 bit/000000${i}${j}.bit" "bin/000000${i}${j}.bin"
  }
}
