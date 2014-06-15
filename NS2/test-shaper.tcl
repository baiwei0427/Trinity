#Network Simulator
set ns [new Simulator]

set N 4 
set RTT 0.0001
set packetSize 1460
set lineRate 1Gb
set ackRatio 1 
set runTimeInSec 1
set intervalInSec 0.1; #Measurement interval in second

Agent/TCP set packetSize_ $packetSize
Agent/TCP/FullTcp set segsize_ $packetSize
Agent/TCP set window_ 50
Agent/TCP set slow_start_restart_ false
Agent/TCP set tcpTick_ 0.000001; # Timer granularity = 1us 
Agent/TCP set minrto_ 0.0001 ; # minRTO = 100us
Agent/TCP set windowOption_ 0
Agent/TCP/FullTcp set segsperack_ $ackRatio; 
Agent/TCP/FullTcp set spa_thresh_ 3000;
Agent/TCP/FullTcp set interval_ 0.04 ; #delayed ACK interval = 40ms

Queue set limit_ 1000

set mytracefile [open mytracefile.tr w]
$ns trace-all $mytracefile

set throughputfile [open throughputfile.tr w]

proc finish {} {
	global ns mytracefile throughputfile
	$ns flush-trace
	close $mytracefile
    close $throughputfile
	exit 0
}

#There are three nodes in this simulation
set sender [$ns node]
set switch [$ns node]
set receiver [$ns node]

#Construct the simulation topology: sender--switch--receiver  
$ns duplex-link $sender $switch $lineRate [expr $RTT/4] DropTail
$ns duplex-link $switch $receiver $lineRate [expr $RTT/4] DropTail

#Initialize shaper
set shaper [new SHAPER]
$shaper set queue_num_ 4
$shaper set rate_0_ 350mbit
$shaper set rate_1_ 300mbit
$shaper set rate_2_ 200mbit
$shaper set rate_3_ 100mbit

$shaper set bucket_0_ 80000; #80Kbit=10KB bucket 
$shaper set bucket_1_ 80000
$shaper set bucket_2_ 80000
$shaper set bucket_3_ 80000

$shaper set qlen_0_ 1000
$shaper set qlen_1_ 1000
$shaper set qlen_2_ 1000
$shaper set qlen_3_ 1000

for {set i 0} {$i < $N} {incr i} {
	set tcp($i) [new Agent/TCP/FullTcp/Sack]
	set sink($i) [new Agent/TCP/FullTcp/Sack]
	$sink($i) listen
	
	$ns attach-shaper-agent $sender $tcp($i) $shaper
    $ns attach-agent $receiver $sink($i)

	$tcp($i) set tenantid_ [expr $i]
    $sink($i) set tenantid_ [expr $i];#[expr $i]

    $ns connect $tcp($i) $sink($i)
	
	set ftp($i) [new Application/FTP]
	$ftp($i) attach-agent $tcp($i)
	$ftp($i) set type_ FTP
	$ns at [expr 0.1] "$ftp($i) start"
}

proc record {} {
	global ns throughputfile intervalInSec N tcp sink 
	
	#Get the current time
	set now [$ns now]
	
	#Initialize the output string 
	set str $now
	
	for {set i 0} {$i < $N} {incr i} {
		set bw($i) [$tcp($i) set bytes_]
		append str " "
		append str [expr int($bw($i)/$intervalInSec*8/1000000)]
		$tcp($i) set bytes_ 0	
	}
	puts $throughputfile $str 
	
	#Set next callback time
	$ns at [expr $now+$intervalInSec] "record"
	
}

$ns at 0.1 "record"
$ns at $runTimeInSec "finish"
$ns run





















